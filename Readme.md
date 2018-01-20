#ScreenCast
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
Output data is sent via UDP datagram to the specified IP:port as individual RGBA bytes.