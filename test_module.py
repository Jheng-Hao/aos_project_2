import os
from os import listdir

def ParseLine (line):
        if not line:
                return None

        tokens = line.split()
        if 0 == len(tokens):
                return None

        tag = tokens[0]

        tokens = tag.split('_');
        if 2 != len(tokens):
                return None

        ret_val = []
        if tokens[0].startswith('w'):
                ret_val.append('w')
                try:
                        value = int(tokens[0][1:])
                except ValueError:
                        return None
                ret_val.append(value)
        elif tokens[0].startswith('r'):
                ret_val.append('r')
                try:
                        value = int(tokens[0][1:])
                except ValueError:
                        return None
                ret_val.append(value)
        else:
                return None

        if "start" == tokens[1]:
                ret_val.append('s')
        elif "end" == tokens[1]:
                ret_val.append('e')
        else:
                return None

        return ret_val
                

file_name = "log"
exclusion_satisfied = True
with open(file_name, 'r') as f:
        while True:
                line = f.readline()
                if not line:
                        break

                ret_val = ParseLine(line)
                if ret_val is None:
                        print "unhandled line: " + line

                if 'w' == ret_val[0]:
                        if 's' != ret_val[2]:
                                exclusion_satisfied = False
                                break

                        next_line = f.readline()
                        if not next_line:
                                break

                        next_ret_val = ParseLine(next_line)
                        if next_ret_val is None:
                                exclusion_satisfied = False
                                break

                        if 'w' != next_ret_val[0]:
                                exclusion_satisfied = False
                                break

                        if ret_val[1] != next_ret_val[1]:
                                exclusion_satisfied = False
                                break

                        if 'e' != next_ret_val[2]:
                                exclusion_satisfied = False
                                break

                        continue

        if exclusion_satisfied:
                print "exclusion satisfied"
        else:
                print "exclusion violated"



