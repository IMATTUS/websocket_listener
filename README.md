# Websocket_listener

I've created this listener to receive commands over websocket on a json formated string.

It can set and get values and issue commands.
#

Check [Command Web Page](https://github.com/IMATTUS/websocket_listener/blob/master/command_websocket.html) for examples of set and get commands.  

#
You'll need libwebsockets and pthreads library
#
For working with json I used:
https://github.com/DaveGamble/cJSON

For working with websockets I used this code:
https://gist.github.com/martinsik/3654228
