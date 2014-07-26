using namespace std;
#include <list>
#include <cstdlib>
#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <fstream>
#include <map>
#include <netdb.h>
#include <sys/types.h>
#include <queue>
#include <iostream>

// Global: my File replica
class myFile_c{
public:
	std::string str;
	int version;
	myFile_c(){
		version=1;
		str="my initial file \n";
	}
};
myFile_c myFile;
pthread_mutex_t myFileLock= PTHREAD_MUTEX_INITIALIZER;

// global Data
std::list<int> neighbours;
int myNode;
int readQuorumSize;
int writeQuorumSize;
bool STOP_flag=false;

// Read Write Lock
pthread_rwlock_t  rwLock = PTHREAD_RWLOCK_INITIALIZER;

//global struct of RDWR_REQ send
class rdWr_req_send_c{
public:
	int seq;
	int highestVer;
	int senderHighestVer;
	int needed;
	std::vector<int> grantedNodes;
	rdWr_req_send_c(){
		seq=-1;highestVer=-1;senderHighestVer=-1;needed=0xfffffff;
	}
	rdWr_req_send_c(int s,int hV, int gN){
		seq=s;highestVer=hV;senderHighestVer=gN;
	}
	int destroy(){
		seq=-1;highestVer=-1;grantedNodes.clear();needed=0xfffffff;
	}
	int eraseSeq(){
		seq=-1;
	}
};
rdWr_req_send_c rdWr_req_send;
pthread_mutex_t rdWr_req_sendLock= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t rdWr_req_sendCondVar = PTHREAD_COND_INITIALIZER;

// global send socket descriptors
std::map<int,int> sendSockDes;

// global outgoing "Seq number"
int outSeqNum=1;

// Init Mutex Lock
pthread_mutex_t initLock=PTHREAD_MUTEX_INITIALIZER;

//send mutex
pthread_mutex_t sendLock=PTHREAD_MUTEX_INITIALIZER;

//Update sync mutex and cond variable
pthread_mutex_t updateSyncLock=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t updateSyncCondVar=PTHREAD_COND_INITIALIZER;

void* send(void* p){
	//cout<<"in send\n";

	int connSock, in, i, ret=1, flags;
	struct sockaddr_in servaddr;
	struct sctp_status status;
	struct sctp_sndrcvinfo sndrcvinfo;
	struct sctp_event_subscribe events;
	struct sctp_initmsg initmsg;
	char buffer[1025];

	//for(int i=0;i<neighbours.size();i++){
	for(std::list<int>::iterator it=neighbours.begin(); it!=neighbours.end(); it++){
		char destAdd[100];
		struct hostent *host;

		sprintf(destAdd,"net%02d.utdallas.edu",*it);

		cout<<"destAdd:"<<destAdd<<"\n";
		connSock = socket( AF_INET, SOCK_STREAM, IPPROTO_SCTP );

		/* Specify that a maximum of 5 streams will be available per socket */
		memset( &initmsg, 0, sizeof(initmsg) );
		initmsg.sinit_num_ostreams = 1;
		initmsg.sinit_max_instreams = 1;
		initmsg.sinit_max_attempts = 4;
		ret = setsockopt( connSock, IPPROTO_SCTP, SCTP_INITMSG,&initmsg, sizeof(initmsg) );

		/* Specify the peer endpoint to which we'll connect */
		bzero( (void *)&servaddr, sizeof(servaddr) );
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons(59000);
		//servaddr.sin_addr.s_addr = inet_addr( "127.0.0.1" );

		host=gethostbyname(destAdd);
		memcpy(&servaddr.sin_addr,host->h_addr_list[0],host->h_length);

		/* Connect to the server */
		while(connect( connSock, (struct sockaddr *)&servaddr, sizeof(servaddr) )<0){
			//perror("error in connection\n");
			sleep(1);
		}

		sendSockDes[*it]=connSock;
	}
	cout<<"all outgoing connections established\n";
	//mut=1;
	for(map<int,int>::iterator iter=sendSockDes.begin();iter!=sendSockDes.end();iter++){
		cout<<iter->first<<":"<<iter->second<<"\n";
	}
	pthread_mutex_unlock(&initLock);
}
int sendUPDATE_REP(int destNode){

	char* temp=new char[200+myFile.str.size()]();
	sprintf(temp,"Sender:%02d Type:UPDATE_REP Version:%d Msg:%s",myNode,myFile.version, myFile.str.c_str() );

	pthread_mutex_lock(&sendLock);
	if(send(sendSockDes[destNode],temp,strlen(temp),0)!=strlen(temp)){
		perror("cant send UPDATE_REQ msg");
	}
	pthread_mutex_unlock(&sendLock);
	delete(temp);
}	

int sendUPDATE_REQ(int destNode){

	char temp[100];
	sprintf(temp,"Sender:%02d Type:UPDATE_REQ",myNode);

	pthread_mutex_lock(&sendLock);
	if(send(sendSockDes[destNode],temp,strlen(temp),0)!=strlen(temp)){
		perror("cant send UPDATE_REQ msg");
	}
	pthread_mutex_unlock(&sendLock);
}

int sendRELEASE(int destNode){
	char temp[100];
	sprintf(temp,"Sender:%02d Type:RELEASE",myNode);

	pthread_mutex_lock(&sendLock);
	if(send(sendSockDes[destNode],temp,strlen(temp),0)!=strlen(temp)){
		perror("cant send RELEASE msg");
	}
	pthread_mutex_unlock(&sendLock);
}
int sendGRANT(int destNode,int seq_in){
	char temp[200];
	sprintf(temp,"Sender:%02d Seq:%d Type:GRANT Version:%d",myNode,seq_in,myFile.version);

	pthread_mutex_lock(&sendLock);
	if(send(sendSockDes[destNode],temp,strlen(temp),0)!=strlen(temp)){
		perror("cant send GRANT msg");
	}
	pthread_mutex_unlock(&sendLock);
}

void* nodeServer(void* p){
	// MSG format: 
	// Sender:<node> Seq:<seq no> Type:<type> Version:<version no> TS:<logical clk> Msg: "xxxxx"
	// <type>= READ_REQ, WRITE_REQ, UPDATE_REQ, UPDATE_REP, GRANT, RELEASE

	int i;
	i=*(int*)p;
	while(1){
		char* buf=new char[1025]();
		int numRcv=1;
		int senderId=-1,ts=-1,seq=-1,version=-1;
		do{
			numRcv=recv(i,buf,1025,0);
		}while(numRcv<=0);

		cout<<"---------\n"<<buf<<"\n";

		//parse message
		char* TS=strstr(buf,"TS:");
		if(TS!=NULL){
			ts=atoi(TS+strlen("TS:"));	
		}

		char* sender_ptr=strstr(buf,"Sender:");
		if(sender_ptr!=NULL){
			senderId=atoi(sender_ptr+strlen("Sender:"));	
		}

		char* seq_ptr=strstr(buf,"Seq:");
		if(seq_ptr!=NULL){
			seq=atoi(seq_ptr+strlen("Seq:"));	
		}
		char* version_ptr=strstr(buf,"Version:");
		if(version_ptr!=NULL){
			version=atoi(version_ptr+strlen("Version:"));
		}

		char* type=strstr(buf,"Type:");
		if (type!=NULL){
			type=type+strlen("Type:");

			// if "READ REQ" message
			if(strstr(type,"READ_REQ")==type){

				if(STOP_flag!=true){

					//reply with GRANT message
					//lock will be released when u get RELEASE msg in SAME thread	
					int ret=pthread_rwlock_tryrdlock(&rwLock);
					if (0==ret){
						sendGRANT(senderId,seq);
					}
				}
			}

			// if "WRITE_REQ" message
			else if(strstr(type,"WRITE_REQ")==type){
				
				if(STOP_flag!=true){
					int ret=pthread_rwlock_trywrlock(&rwLock);
					if (0==ret){
						sendGRANT(senderId,seq);
					}
				}	
			}
			// if "GRANT" message
			else if(strstr(type,"GRANT")==type){

				//update rdWr_req_send
				pthread_mutex_lock(&rdWr_req_sendLock);

				if(seq!=rdWr_req_send.seq){
					// unsolicited reply or late reply: send RELEASE message
					sendRELEASE(senderId);
				}
				else{

					//if update highest version received
					if(rdWr_req_send.highestVer<version){
						rdWr_req_send.highestVer=version;
						rdWr_req_send.senderHighestVer=senderId;
					}
					rdWr_req_send.grantedNodes.push_back(senderId);

					//check if read quorum is fulfilled
					if(rdWr_req_send.grantedNodes.size()>=rdWr_req_send.needed){
						//clear rdWr_req_send.Seq and wake up reading
						rdWr_req_send.eraseSeq();
						pthread_cond_signal(&rdWr_req_sendCondVar);
					}
				}
				pthread_mutex_unlock(&rdWr_req_sendLock);
			}

			//if "UPDATE_REQ" message
			else if(strstr(type,"UPDATE_REQ")==type){
				//TO DO: we can check if already locked, if not this is Error
				// this thread should already have a lock, so read and send
				sendUPDATE_REP(senderId);
			}

			//if "UPDATE_REP" message
			else if(strstr(type,"UPDATE_REP")==type){
				// update in this thread, its possibly safe!
				// update myfile with this Msg:

				pthread_mutex_lock(&updateSyncLock);

				std::string msgSave;
				char* msg_ptr=strstr(buf,"Msg:");
				if(msg_ptr==NULL){
					cout<<"Error, REQ message received with no Msg\n\n\n\n";
				}
				msg_ptr=msg_ptr+strlen("Msg:");
				msgSave=msg_ptr;
				myFile.str=msgSave;
				myFile.version=version;
				
				pthread_cond_signal(&updateSyncCondVar);

				pthread_mutex_unlock(&updateSyncLock);

			}

			//if "RELEASE" message
			else if(strstr(type,"RELEASE")==type){
				pthread_rwlock_unlock(&rwLock); 

			}

			else{
				cout<<"Invalid Type received\n";
			}
		}
		else{
			cout<<"Invalid message received\n";
		}
		delete(buf);
	}
}

void* receive(void* p){

	//cout<<"in receive\n";

	int listenSock, connSock, ret;
	struct sockaddr_in servaddr;
	char buffer[1025];

	listenSock = socket( AF_INET, SOCK_STREAM, IPPROTO_SCTP );

	bzero( (void *)&servaddr, sizeof(servaddr) );
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl( INADDR_ANY );
	servaddr.sin_port = htons(59000);

	if(0!=bind( listenSock,(struct sockaddr *)&servaddr, sizeof(servaddr) )){
		perror("bind error");
		exit(0);
	}	
	listen( listenSock, 10);

	//for(int n=0;n<neighbours.size();n++){
	while(1){
		pthread_t nodeServerId;
		int ret;
		connSock = accept( listenSock,(struct sockaddr *)NULL, NULL );

		cout<<"----------Accepted----------\n";

		ret=pthread_create(&nodeServerId,NULL,nodeServer,&connSock);
		if(ret!=0){
			cout<<"Node server thread creation failed";
			exit(0);
		}
		//close( connSock );
	}
}
int readTopologyFile(std::list<int>& neighbourIn, int& myNodeIn){
	ifstream topology("topology");
	std::string str;
	char* cStr;
	char hName[100];
	if (topology.is_open()){
		getline(topology,str);
	}
	else cout<<"cant open topology file\n";

	if (0!=gethostname(hName, 100)){
		cout<<"error in gettting hostname\n";
	}
	cout<<"hostname="<<hName<<"\n";

	char myNode[3];
	strncpy(myNode,hName+3,3);
	myNode[2]='\0';
	myNodeIn=atoi(myNode);
	char* read;
	int nNode;
	cStr=new char[str.length()+1]();
	strcpy(cStr,str.c_str());
	//cout<<"cStr="<<cStr;
	read=strtok(cStr," ");
	while(read!=NULL){
		nNode=atoi(read);
		if (nNode!=myNodeIn){
			neighbourIn.push_back(nNode);
		}	
		read=strtok(NULL," ");
	}
	writeQuorumSize=(neighbourIn.size()+1)/2 +1;
	readQuorumSize=(neighbourIn.size()+1) - writeQuorumSize +1;
}

int init(int argc, char* argv[]){

	pthread_mutex_lock(&initLock);

	readTopologyFile(neighbours,myNode);

	cout<<"\nneighbours\n";
	for(list<int>::iterator iter=neighbours.begin();iter!=neighbours.end();iter++) {
		cout<<*iter<<"\n";
	}

	cout<<"myNode="<<myNode<<"\n";
	cout<<"read Quorum= "<<readQuorumSize<<"  write Quorum= "<<writeQuorumSize<<endl;
	pthread_t sendThd, rcvThd;
	int ret1, ret2;

	ret1=pthread_create(&sendThd,NULL,send,NULL);
	if(ret1!=0){
		cout<<"send thread creation failed";
		exit(0);
	}

	ret2=pthread_create(&rcvThd,NULL,receive,NULL);
	if(ret2!=0){
		cout<<"receive thread creation failed";
		exit(0);
	}
	//send(NULL);

	if(0!=pthread_join(sendThd,NULL)){
		perror("Error in joining sendThd");
	}	
	//pthread_join(rcvThd,NULL);

	//A bad hack!
	sleep(2);

	return 1;
}

int sendToAll(char *p_in){
	for(list<int>::iterator iter=neighbours.begin();iter!=neighbours.end();iter++) {
		pthread_mutex_lock(&sendLock);
		if(send(sendSockDes[*iter],p_in,strlen(p_in),0)!=strlen(p_in)){
			perror("cant send clock");
		}
		pthread_mutex_unlock(&sendLock);
	}
}
void* runSleepTimer(void*){
	usleep(1000000);
	pthread_cond_signal(&rdWr_req_sendCondVar);
}
pthread_t runTimer(){
	// start timer and run 
	pthread_t timerId;
	int ret=pthread_create(&timerId,NULL,runSleepTimer,NULL);
	if(ret!=0){
		cout<<"Node server thread creation failed";
		exit(0);
	}
	return timerId;
}

int m_read(std::string& str_in, int file_number=0){
	int ret;
	pthread_t child;

	//lock your own copy
	ret=pthread_rwlock_tryrdlock(&rwLock );
	
	if(0!=ret){
		perror("can't get Read Lock");
		return ret;
	}

	//send READ_REQ message and get locks on all
	char ptr[100];
	sprintf(ptr,"Sender:%02d Seq:%d Type:READ_REQ",myNode,outSeqNum);

	pthread_mutex_lock(&rdWr_req_sendLock);	

	rdWr_req_send.seq=outSeqNum;
	rdWr_req_send.needed=readQuorumSize-1;
	outSeqNum++;
	sendToAll(ptr);
	
	child=runTimer();
	
	pthread_cond_wait(&rdWr_req_sendCondVar,&rdWr_req_sendLock);
	
	//check if timer expired or you got enough GRANTS
	if(rdWr_req_send.grantedNodes.size()>=rdWr_req_send.needed){
		//kill timer thread
		int cancelRet=pthread_cancel(child);
		if (0!=cancelRet){
			perror("Cant Kill Timer thread");
		}
	}
	else{
		cout<<"Timer Expired!!!"<<endl;
		//release all copies
		for(std::vector<int>::iterator i=rdWr_req_send.grantedNodes.begin();i!=rdWr_req_send.grantedNodes.end();i++){
			sendRELEASE(*i);	
		}
		rdWr_req_send.destroy();
		pthread_mutex_unlock(&rdWr_req_sendLock);
		pthread_rwlock_unlock(&rwLock );
		return -1;
	}
	//check if stale version
	if(myFile.version < rdWr_req_send.highestVer){
		// use conditional variable to wait till thread completes
		pthread_mutex_lock(&updateSyncLock);
		sendUPDATE_REQ(rdWr_req_send.senderHighestVer);
		pthread_cond_wait(&updateSyncCondVar, &updateSyncLock);
		pthread_mutex_unlock(&updateSyncLock);
	}

	//read your copy
	str_in=myFile.str;

	//release all copies
	for(std::vector<int>::iterator i=rdWr_req_send.grantedNodes.begin();i!=rdWr_req_send.grantedNodes.end();i++){
		sendRELEASE(*i);
	}
	rdWr_req_send.destroy();
	pthread_mutex_unlock(&rdWr_req_sendLock);

	//UNlock your own copy
	pthread_rwlock_unlock(&rwLock );

	cout<<"my file contents:- "<<endl<<str_in<<endl;
	cout<<"m_read DONE-----------------------------------------"<<endl;
	return 0;
}

int m_write(std::string str_in, int file_number=0){
	
	pthread_t child;
	int ret;
	//lock your own copy
	ret=pthread_rwlock_trywrlock(&rwLock );

	if(0!=ret){
		perror("can't get Write Lock");
		return ret;
	}

	//send WRITE_REQ message and get locks on all
	char ptr[100];
	sprintf(ptr,"Sender:%02d Seq:%d Type:WRITE_REQ",myNode,outSeqNum);

	pthread_mutex_lock(&rdWr_req_sendLock);	

	rdWr_req_send.seq=outSeqNum;
	rdWr_req_send.needed=writeQuorumSize-1;
	outSeqNum++;
	sendToAll(ptr);

	child=runTimer();
	
	pthread_cond_wait(&rdWr_req_sendCondVar,&rdWr_req_sendLock);
	
	//check if timer expired or you got enough GRANTS
	if(rdWr_req_send.grantedNodes.size()>=rdWr_req_send.needed){
		//kill timer thread
		int cancelRet=pthread_cancel(child);
		if (0!=cancelRet){
			perror("Cant Kill Timer thread");
		}
	}
	else{
		cout<<"Timer Expired!!!"<<endl;
		//release all copies
		for(std::vector<int>::iterator i=rdWr_req_send.grantedNodes.begin();i!=rdWr_req_send.grantedNodes.end();i++){
			sendRELEASE(*i);	
		}
		rdWr_req_send.destroy();
		pthread_mutex_unlock(&rdWr_req_sendLock);
		pthread_rwlock_unlock(&rwLock );
		return -1;
	}

	//check if stale version
	if(myFile.version<rdWr_req_send.highestVer){
		// use conditional variable to wait till thread completes
		pthread_mutex_lock(&updateSyncLock);
		sendUPDATE_REQ(rdWr_req_send.senderHighestVer);
		pthread_cond_wait(&updateSyncCondVar, &updateSyncLock);
		pthread_mutex_unlock(&updateSyncLock);
	}

	//append your copy
	myFile.version++;
	char temp[100];
	sprintf(temp,"Version %d: ",myFile.version);
	myFile.str.append(temp);
	myFile.str.append(str_in);

	//send update response to all copies
	for(std::vector<int>::iterator i=rdWr_req_send.grantedNodes.begin();i!=rdWr_req_send.grantedNodes.end();i++){
		sendUPDATE_REP(*i);
	}
	//release all copies
	for(std::vector<int>::iterator i=rdWr_req_send.grantedNodes.begin();i!=rdWr_req_send.grantedNodes.end();i++){
		sendRELEASE(*i);	
	}
	rdWr_req_send.destroy();
	pthread_mutex_unlock(&rdWr_req_sendLock);

	//UNlock your own copy
	pthread_rwlock_unlock(&rwLock );
	cout<<"m_write DONE-----------------------------------------"<<endl;

	return 0;
}

/*
int main (int argc, char* argv[]){

	init(argc,argv);

	pthread_mutex_lock(&initLock);

	if (argc >1){
		STOP_flag=true;
		//m_read();
	}
	char dummy;
	std::string dummy2;
	while(1){
		cin>>dummy;
		if(dummy=='w'){
			cout<<"Write command:"<<endl;
			m_write("Dummy\n");
		}
		else{
			cout<<"Read command:"<<endl;
			//while(1)
				m_read(dummy2);
		}	
	}

	//LogFile.close();
}
*/

