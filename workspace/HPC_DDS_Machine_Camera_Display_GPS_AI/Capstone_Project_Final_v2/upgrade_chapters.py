import os
import re

directory = '/home/huytai102/AUTOSAR_AP-ADAS/workspace/HPC_DDS_Machine_Camera_Display_GPS_AI/Capstone_Project_Final/Contents'

def replace_in_file(filepath):
    with open(filepath, 'r') as file:
        content = file.read()
    
    # We must replace from largest to smallest to avoid collisions
    # subsubsubsection -> subsubsection
    content = content.replace(r'\subsubsubsection', r'\subsubsection')
    # subsubsection -> subsection
    content = content.replace(r'\subsubsection', r'\subsection')
    # subsection -> section
    content = content.replace(r'\subsection', r'\section')
    # section -> chapter
    # BUT wait, the ones that were just converted to \section will now become \chapter!
    # So we must use a regex with a function or use temporary tokens.

    def replacer(match):
        cmd = match.group(1)
        if cmd == 'section': return r'\chapter'
        if cmd == 'subsection': return r'\section'
        if cmd == 'subsubsection': return r'\subsection'
        if cmd == 'subsubsubsection': return r'\subsubsection'
        return match.group(0)
        
    new_content = re.sub(r'\\(section|subsection|subsubsection|subsubsubsection)', replacer, content)
    
    if new_content != content:
        with open(filepath, 'w') as file:
            file.write(new_content)
        print(f'Updated {filepath}')

for root, _, files in os.walk(directory):
    for file in files:
        if file.endswith('.tex'):
            replace_in_file(os.path.join(root, file))
