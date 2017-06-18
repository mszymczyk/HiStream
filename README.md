# HiStream - binary, hierarchical file format

Game data is most efficiently stored as a binary blob. We get fast reads and compact size. The downside is that debugging contents of such  file is difficult. We have to write separate piece of code for each file type to extract it's contents for inspection.

On the other side of spectrum, there are text files, like XML or JSON, that are easy to read but offer less read performance and have usually bigger (and non deterministic) sizes.

This is where HiStream comes in. It's binary file, but offers hierarchical structure like XML or JSON. It consists of nodes and attributes. Each node and attribute contains some metadata that can be used to query basic informations about stored data. 
There's utility, hisconv.exe, that automatically converts between binary and XML formats. Any file can be decoded to XML. It can then be inspected manually or programatically (every language out there offers good support for XML). It's contents may be changed or generated from scratch and then encoded back to binary. Game code needs to worry only about binary form.

There are some downsides. Each node and attribute needs extra space for metadata (4 bytes for an attribute, 24 bytes for a node). There may be few bytes of padding added to node and attribute, so data is naturally aligned on memory for fast reading. This causes HiStream files to be slighly bigger than their 'native' equivalents.

HiStream can be used to store any kind of game data: triangle meshes, animations, cutscenes, configuration files, level descriptions, save data, ...

HiStream can be used in asset pipeline as an intermediate file format as well as target format to be read directly by game engine.

