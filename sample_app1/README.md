# DeepStream Tutorial Series

## Building Your First GStreamer Pipeline

This pipeline accepts any mp4 file source as input and performs inference using the model provided in the the DeepStream Reference application1 and saves the output to an output file.

<a href="https://imgur.com/1apPxeC"><img src="https://i.imgur.com/1apPxeC.png" title="Pipeline" /></a>

## Instructions

Set CUDA_VER in Makefile. Check the cuda version by using the following command
```
$ nvcc --version
```

To Compile the code
```
$ make
```
To run the code
```
./deepstream_tutorial_app1 /opt/nvidia/deepstream/deepstream/samples/streams/sample_720p.mp4
```