# ScreenCast
A small utility to send raw screen data to a network connected display.

## Usage
Start ScreenCast.exe and resize and move the window to cover the desired area of screen to be sent.

The Connect menu will allow you to configure:
- IP address
- UDP Port
- The size of the output image (HxW)
- Framerate to send

Once the cast has begun, the application will resize the monitored area to the specified HxW and generate UDP packets at the specified framerate.

## Output format
Output data is sent one frame at a time, via UDP datagram to the specified IP:port.

Each frame consists of the following data:

`UINT16 Ident` **Frame Identifier**: Short preamble to identify this as a screencast packet (default 0x1234)

`UINT16 height` **Frame Height**: Height of the frame, used by client to make sure data format is consistent

`UINT16 width` **Frame Width**: Width of the frame, used by client to make sure data format is consistent

`UINT16 length` **Data Length**: Length of the following pixel data, used by client to make sure data format is consistent

`UINT8 data[]` **Pixel Data**: The actual screen contents. 4 bytes per pixel, R, G, B, A.

Pixels are ordered starting in the bottom left, going horizontally:
```
    5  6  7  8 
	1  2  3  4
```