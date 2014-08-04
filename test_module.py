import os
from os import listdir

def ParseLine (line):
        if not line:
                return None

        ret_val = []
        if line.startswith("Write Start"):
                ret_val.append("ws")
        elif line.startswith("Write End"):
                ret_val.append("we")
        elif line.startswith("Read Start"):
                ret_val.append("rs")
        elif line.startswith("Read End"):
                ret_val.append("re")
        else:
                return None


        version_tag = "Version "
        version_idx = line.find(version_tag)
        if -1 == version_idx:
                return None

        version_idx = version_idx + len(version_tag)
        try:
                version = int(line[version_idx:])
        except ValueError:
                return None
        ret_val.append(version)

        return ret_val
                

file_name = "logFile.txt"
exclusion_satisfied = True
with open(file_name, 'r') as f:
        latest_version_number = 1
        err_msg = ""
        read_start_count = 0
        read_end_count = 0

        while True:
                line = f.readline()
                if not line:
                        break

                ret_val = ParseLine(line)
                if ret_val is None:
                        print "unhandled line: " + line
                        continue

                if "ws" == ret_val[0]:
                        if read_start_count != read_end_count:
                                print "[WARNING] Read Start and Read End are not consistent"
                                break

                        next_line = f.readline()
                        if not next_line:
                                break

                        next_ret_val = ParseLine(next_line)
                        if next_ret_val is None:
                                print "unhandled line: " + line
                                exclusion_satisfied = False
                                err_msg = "not Write End"
                                break

                        if "we" != next_ret_val[0]:
                                exclusion_satisfied = False
                                err_msg = "not Write End"
                                break

                        # Write Start Version < Write End Version
                        if ret_val[1] >= next_ret_val[1]:
                                exclusion_satisfied = False
                                print "Write Start Vertion: " + str(ret_val[1])
                                print "Write End Version: " + str(next_ret_val[1])
                                err_msg = "invalid version number"
                                break

                        # Write End Version is strict monotonic
                        if next_ret_val[1] <= latest_version_number:
                                exclusion_satisfied = False
                                print "Latest Version: " + str(latest_version)
                                print "Write End Version: " + str(next_ret_val[1])
                                err_msg = "version number is not strict monotonic"
                                break

                        latest_version_number = next_ret_val[1]
                        continue
                elif "rs" == ret_val[0]:
                        read_start_count = read_start_count + 1
                        if latest_version_number != ret_val[1]:
                                print "[WARNING] did not read the latest version"
                elif "re" == ret_val[0]:
                        read_end_count = read_end_count + 1
                        if latest_version_number != ret_val[1]:
                                print "[WARNING] did not read the latest version"
                else:
                        print "unhandled tag: " + ret_val[0]
                        print line

        if exclusion_satisfied:
                print "exclusion satisfied"
        else:
                print "exclusion violated"
                print err_msg



