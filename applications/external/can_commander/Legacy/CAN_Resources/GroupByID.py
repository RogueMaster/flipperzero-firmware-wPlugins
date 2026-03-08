import re
from collections import defaultdict

def group_logs_by_id(input_file, output_file):
    log_dict = defaultdict(list)
    
    
    id_regex = re.compile(r"ID: ([0-9A-Fa-f]+)")
    
    with open(input_file, 'r') as infile:
        for line in infile:
            match = id_regex.search(line)
            if match:
                can_id = match.group(1)
                log_dict[can_id].append(line.strip())
    

    with open(output_file, 'w') as outfile:
        for can_id in sorted(log_dict.keys()):
            outfile.write(f"Group ID: {can_id}\n")
            for entry in log_dict[can_id]:
                outfile.write(entry + "\n")
            outfile.write("\n")  

input_file = 'logfile.txt'
output_file = 'grouped_logfile.txt'
group_logs_by_id(input_file, output_file)
