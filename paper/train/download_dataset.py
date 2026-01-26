# !pip install roboflow

from roboflow import Roboflow
rf = Roboflow(api_key="UcTwb0lox8uPatTjxmVm")
project = rf.workspace("huytai102").project("traffic-light-v9orl-onmce")
version = project.version(1)
dataset = version.download("coco")
