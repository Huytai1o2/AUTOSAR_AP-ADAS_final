import os

directory = '/home/huytai102/AUTOSAR_AP-ADAS/workspace/HPC_DDS_Machine_Camera_Display_GPS_AI/Capstone_Project_Final/Intro'

def replace_in_file(filepath):
    with open(filepath, 'r') as file:
        content = file.read()
    
    new_content = content.replace(r'\section*', r'\chapter*')
    
    if new_content != content:
        with open(filepath, 'w') as file:
            file.write(new_content)
        print(f'Updated {filepath}')

for root, _, files in os.walk(directory):
    for file in files:
        if file.endswith('.tex'):
            replace_in_file(os.path.join(root, file))
