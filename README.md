# Direct to Video (DTV)

![Alt text](docs/public/dtv_banner.png?raw=true)

## What does DTV do?
DTV is a library that provides a simple interface for generating video files from a C++ application. It uses FFmpeg which means that it supports the same encoders and performance that you would get from something like OBS. Unlike OBS, however, it is not dependent on the input being delivered in real-time. This means that regardless of the input frequency to DTV, the output will always be at the frame rate that you select.

## How do I use it?
Check out the sample application that is provided with DTV. The interface is extremely minimalistic and is comprised of a handful of intuitive functions. The steps to write an entire video look like this:

1. Start the encoder by calling the ```encoder.run(...)``` method. The encoder runs in its own thread and will wait for you to send video frames to it.
2. Call ```encoder.newFrame(...)``` to get a frame from the buffer to write to. This comes in blocking and non-blocking versions. By calling ```encoder.newFrame(true)```, DTV will wait for a frame to become available if the encoder is lagging behind the input. Changing the input to ```false``` will cause the encoder to return ```nullptr``` if a frame is not available to write to. This can be useful if you'd prefer to miss frames over slowing down your application.
3. Fill the frame with whatever data you like by populating the ```atg_dtv::Frame::m_rgb``` array with 8-bit values.
4. Commit the frame by calling ```encoder.submitFrame()```.
5. Repeat steps 2-4 until you have no frames left to write.
6. Call ```encoder.commit()``` to inform the encoder that the video stream is over.
7. Call ```encoder.stop()``` which will wait until the encoder finishes encoding buffered frames and then for the encoder thread to exit.

## How do I build it?
You will need to have FFmpeg development libraries installed on your computer and the directory listed on your PATH. DTV has only been tested on Windows but in principle should build on other platforms. The cmake script that searches for FFmpeg libraries, however, is Windows specific and you'll have to modify ```cmake/FindFFmpeg.cmake``` to work for other platforms. (If you do this, please create a pull-request!)

For FFmpeg pre-built binaries, see the [OBS installation instructions](https://github.com/obsproject/obs-studio/wiki/Install-Instructions) page.

### Build Steps

1. Make sure you have the latest version of CMake installed
2. Make sure you have FFmpeg binaries installed (see above)
3. Clone the repository: ```git clone https://github.com/ange-yaghi/direct-to-video.git```
4. ```cd direct-to-video```
5. ```mkdir build```
6. ```cd build```
7. ```cmake ..```
8. ```cmake --build .```
9. For MSVC users, check the build folder for a generated Visual Studio solution. You can use this solution like any other and will contain the DTV library as well as the demo application in separate projects.
