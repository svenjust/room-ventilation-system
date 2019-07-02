# Screenshot Service

It is possible to get screenshots from the TFT screen via network. The controller
can connect to a specified IP address and port 4444 to send the screenshot.

You can use for instance netcat program to receive a screenshot:

`nc -l -p 4444 >screenshot.bmp`

Then, send a MQTT message to generate screenshot:

Message                  | Value          | Description
------------------------ | -------------- | ------------------------
`d15/set/kwl/screenshot` | address[:port] | Send screenshot as BMP file.

If no port is specified, port 4444 will be used as default.

Unfortunately, it's not possible to time screenshots perfectly, since it takes
several seconds for the controller to recognize the incoming message.

Screenshot is produced in about 10 seconds from the time at which the message
is received. During this time, the controller is blocked.
