# Coeditor description file
> Ruize Tang
> 2017

```
//TODO: Improve this document
```
## Introduction
The Coeditor is based on Jupiter protocol, which supports long-term remote Collaborative editing.
## Dependency
```
sudo apt-get install libncurses6-dev || sudo apt-get install libncurses5-dev
sudo apt-get install libgpm-dev xinetd realpath
```
## Configuration
```
# compiling
make
# register our server program
sudo ./register_inetd.sh
# now you can run coeditor
./coeditor -c localhost
```
## Usage
Please see 'doc/coeditor.help' file.