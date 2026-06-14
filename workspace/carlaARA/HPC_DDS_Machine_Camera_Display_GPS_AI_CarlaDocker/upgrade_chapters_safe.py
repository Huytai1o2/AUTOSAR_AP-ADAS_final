import os
import re

directories = [
    '/home/huytai102/AUTOSAR_AP-ADAS/workspace/HPC_DDS_Machine_Camera_Display_GPS_AI/Capstone_Project_Final/Contents/chapter1',
    '/home/huytai102/AUTOSAR_AP-ADAS/workspace/HPC_DDS_Machine_Camera_Display_GPS_AI/Capstone_Project_Final/Contents/chapter2',
    '/home/huytai102/AUTOSAR_AP-ADAS/workspace/HPC_DDS_Machine_Camera_Display_GPS_AI/Capstone_Project_Final/Contents/chapter3',
    '/home/huytai102/AUTOSAR_AP-ADAS/workspace/HPC_DDS_Machine_Camera_Display_GPS_AI/Capstone_Project_Final/Contents/chapter5'
]

def replace_in_file(filepath):
    with open(filepath, 'r') as file:
        content = file.read()
    
    def replacer(match):
        cmd = match.group(1)
        if cmd == 'section': return r'\chapter'
        if cmd == 'subsection': return r'\section'
        if cmd == 'subsubsection': return r'\subsection'
        if cmd == 'subsubsubsection': return r'\subsubsection'
        return match.group(0)
        
    new_content = re.sub(r'\\(subsubsubsection|subsubsection|subsection|section)', replacer, content)
    
    if new_content != content:
        with open(filepath, 'w') as file:
            file.write(new_content)
        print(f'Updated {filepath}')

for directory in directories:
    for root, _, files in os.walk(directory):
        for file in files:
            if file.endswith('.tex'):
                replace_in_file(os.path.join(root, file))
