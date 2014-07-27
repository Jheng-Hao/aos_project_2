#include <cmath>
#include <ctime>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

extern pthread_mutex_t initLock;
int m_read(std::string& str_in, int file_number=0);
int m_write(std::string str_in, int file_number=0);
int init(int argc, char* argv[]);

namespace
{

const string ErrorHeader = "[Error (application)]";
const int DefaultMean = 5000;
const int DefaultVariance = 5000;
const float DefaultReadRatio = 0.5f;

enum TaskType
{
        kNoTask = 0,
        kRead,
        kWrite,
};

void Parse (const string &line, vector<string> &tokens)
{
        const string Delimiter = " ";

        int start_idx = 0, delimiter_idx;

        while (start_idx < line.length())
        {
                delimiter_idx = line.find(Delimiter, start_idx);

                if (start_idx == delimiter_idx)
                {
                        start_idx += Delimiter.length();
                        continue;
                }

                if (line.npos == delimiter_idx)
                {
                        tokens.push_back(line.substr(start_idx));
                        break;
                }
                else
                {
                        tokens.push_back(line.substr(start_idx, delimiter_idx - start_idx));
                        start_idx += delimiter_idx - start_idx;
                }
        }
}

float UniformRandom ()
{
        static bool init = false;
        if (!init)
        {
                init = true;
                srand(time(NULL));
        }

        return ((float) rand() + 1.0) / ((float) RAND_MAX + 1.0);
}

float NormalRandom (float mean, float variance)
{
        float u1 = UniformRandom();
        float u2 = UniformRandom();
        return mean + variance * cos(2.0 * M_PI * u2) * sqrt(-2.0 * log(u1));
}

void ReadConfig (int &mean, int &variance, float &read_ratio)
{
        ifstream config_file;
        config_file.open("config");
        if (!config_file.is_open())
        {
                cout << ErrorHeader << "can not open config file" << endl;
                return;
        }

        string line;
        vector<string> tokens;
        while (getline(config_file, line))
        {
                tokens.clear();
                Parse(line, tokens);

                if (tokens.size() < 2)
                {
                        cout << ErrorHeader << "incorrect format in config file" << endl;
                        break;
                }

                istringstream iss;
                if ("MEAN" == tokens[0])
                {
                        iss.str(tokens[1]);
                        iss >> mean;
                        if (!iss)
                        {
                                iss.clear();
                                cout << ErrorHeader << "invalid MEAN value" << endl;
                        }

                        if (mean < 0)
                        {
                                cout << ErrorHeader << "MEAN value is less than 0" << endl;
                                mean = DefaultMean;
                        }
                }
                else if ("VARIANCE" == tokens[0])
                {
                        iss.str(tokens[1]);
                        iss >> variance;
                        if (!iss)
                        {
                                iss.clear();
                                cout << ErrorHeader << "invalid VARIANCE value" << endl;
                        }

                        if (variance < 0)
                        {
                                cout << ErrorHeader << "VARIANCE is less than 0" << endl;
                                variance = DefaultVariance;
                        }
                }
                else if ("READ_RATIO" == tokens[0])
                {
                        iss.str(tokens[1]);
                        iss >> read_ratio;
                        if (!iss)
                        {
                                iss.clear();
                                cout << ErrorHeader << "invalid READ_RATIO value" << endl;
                        }

                        if (read_ratio < 0.0f)
                        {
                                cout << ErrorHeader << "READ_RATIO is less than 0" << endl;
                                read_ratio = DefaultReadRatio;
                        }
                        else if (read_ratio > 1.0f)
                        {
                                cout << ErrorHeader << "READ_RATIO is greater than 1" << endl;
                                read_ratio = DefaultReadRatio;
                        }
                }
                else
                {
                        cout << ErrorHeader << "unhandled tag: " << tokens[0] << endl;
                }
        }

        config_file.close();
}

}

int main (int argc, char *argv[])
{
        int mean = DefaultMean;
        int variance = DefaultVariance;
        float read_ratio = DefaultReadRatio;

        ReadConfig(mean, variance, read_ratio);

        cout << "** using: " << endl;
        cout << "**    mean = " << mean << endl;
        cout << "**    variance = " << variance << endl;
        cout << "**    read_ratio = " << read_ratio << endl;

        init(argc, argv);
        pthread_mutex_lock(&initLock);

        TaskType task = kNoTask;
        while (true)
        {
                if (kNoTask == task)
                {
                        if (UniformRandom() < read_ratio)
                        {
                                task = kRead;
                        }
                        else
                        {
                                task = kWrite;
                        }
                }

                static int collision_count = 0;
                bool success = false;
                if (kRead == task)
                {
                        cout << "read file..." << endl;
                        string msg;
                        if (0 == m_read(msg))
                        {
                                success = true;
                        }
                }
                else if (kWrite == task)
                {
                        cout << "write file..." << endl;
                        if (0 == m_write("doing write operation"))
                        {
                                success = true;
                        }
                }
                else
                {
                        cout << ErrorHeader << "unhandled task: " << task << endl;
                }

                int waiting_time;
                if (success)
                {
                        collision_count = 0;
                        waiting_time = NormalRandom(mean, variance);
                        cout << "operation complete: waits " << waiting_time << " (ms)" << endl;
                }
                else
                {
                        ++collision_count;
                        waiting_time = UniformRandom() * (pow(2, collision_count) - 1) * 1000;
                        cout << "colliison: waits " << waiting_time << " (ms)" << endl;
                }

                usleep(waiting_time);
        }

        return 0;
}
