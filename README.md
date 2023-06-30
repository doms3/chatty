# chatty: a command-line interface for OpenAI's chat completion API written in C

## Build requirements
* A POSIX compliant C library and compiler such as GCC or Clang on Linux or MacOS
* CURL
* JSON-C

## Build instructions
Simply run `make` in your terminal to build the `chatty` executable. 
To install, move the created `chatty` executable to a location on your system's path
such as `$HOME/.local/bin`.

## Usage instructions
To begin, run `chatty --help` for a list of all supported ways to use the 
`chatty` executable to interface with OpenAI's chat completion API. 

Note that the environment variable `OPENAI_API_KEY` must be exported with a 
valid API key to use the executable.

## Implementation details
Following the Unix philosophy, in the most common operating mode, the
`chatty` executable accepts input from `stdin` that is send to the 
chat completion API as user input. The program is compliant with the 
"XDG Base Directory" specification and stores its data in the `$XDG_DATA_HOME`
directory.

Chat session history is stored by the application in order to make it easier to
have extended chats.

## TODO
* The `--rollback` flag is currently unimplemented
* The error handling of the `libaichat` sublibrary is quite rudamentary
* It would be nice to be able to save prompts and refer to them by name rather than filename
