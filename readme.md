# Usage
A ns3 add-on that helps design and test novel blockchain systems. It focuses on simulating the behavior of consensus protocols and substrate networking features.

./src is the source code. 

RTWaxman.conf is an example of brite network model config. file

under scratch is an example script of this add-on.


# Build Guide
Tested on linux ubuntu
## 1. get the latest version of ns3
Follow [ns3's official guide](https://www.nsnam.org/wiki/Installation).

## 2. Install Dependencies
Follow [ns3's guide](https://www.nsnam.org/docs/models/html/brite.html) to build the ns-3 specific BRITE repository

Get crypto++

You can get one with the Package Manager, like:
```shell
sudo apt-get install libcrypto++-dev libcrypto++-doc libcrypto++-utils
```
But at the time of writing the Package Manager provides a quite outdated version. So you may want to build it from [source](https://www.cryptopp.com/#download):
```shell
mkdir cryptopp
cd cryptopp/
wget https://www.cryptopp.com/cryptopp820.zip
unzip -a cryptopp820.zip
```
```shell
make static dynamic cryptest.exe
```
```shell
sudo make install PREFIX=/usr/local
sudo mkdir -p /usr/local/include/cryptopp
sudo cp *.h /usr/local/include/cryptopp
sudo chmod 755 /usr/local/include/cryptopp
sudo chmod 644 /usr/local/include/cryptopp/*.h
sudo mkdir -p /usr/local/lib
sudo cp libcryptopp.a /usr/local/lib
sudo chmod 644 /usr/local/lib/libcryptopp.a
sudo mkdir -p /usr/local/bin
sudo cp cryptest.exe /usr/local/bin
sudo chmod 755 /usr/local/bin/cryptest.exe
sudo mkdir -p /usr/local/share/cryptopp
sudo cp -r TestData /usr/local/share/cryptopp
sudo cp -r TestVectors /usr/local/share/cryptopp
sudo chmod 755 /usr/local/share/cryptopp
sudo chmod 755 /usr/local/share/cryptopp/TestData
sudo chmod 755 /usr/local/share/cryptopp/TestVectors
sudo chmod 644 /usr/local/share/cryptopp/TestData/*.dat
sudo chmod 644 /usr/local/share/cryptopp/TestVectors/*.txt
```
```shell
sudo ldconfig
```

## 3. Copy source files 
move all files under /src folder to ns3's /src folder

## 4 Config & Build

### Change config for crypto++

open the wscript file located in your ns3 root folder and search
```python
env = conf.env
```
and add between：
```python
conf.env['lpp'] 
crypto= conf.check(mandatory=True, lib='cryptopp', uselib_store='cryptopp')
conf.env.append_value('CXXDEFINES', 'ENABLE_CRYPTOPP')
conf.env.append_value('CCDEFINES', 'ENABLE_CRYPTOPP')
```
search
```python
if program.env[‘ENABLE_STATIC_NS3’]:
```
for
```python
    if program.env['ENABLE_STATIC_NS3']:
        if sys.platform == 'darwin':
            program.env.STLIB_MARKER = '-Wl,-all_load'
        else:
            program.env.STLIB_MARKER = '-Wl,-Bstatic,--whole-archive'
            program.env.SHLIB_MARKER = '-Wl,-Bdynamic,--no-whole-archive'
    else:
        if program.env.DEST_BINFMT == 'elf':
            # All ELF platforms are impacted but only the gcc compiler has a flag to fix it.
            if 'gcc' in (program.env.CXX_NAME, program.env.CC_NAME): 
                program.env.append_value ('SHLIB_MARKER', '-Wl,--no-as-needed')

return program
```
and modified it to:
```python
    if program.env['ENABLE_STATIC_NS3']:
        if sys.platform == 'darwin':
            program.env.STLIB_MARKER = '-Wl,-all_load,-lcryptopp'
        else:
            program.env.STLIB_MARKER = '-Wl,-Bstatic,--whole-archive,-lcryptopp'
            program.env.SHLIB_MARKER = '-Wl,-Bdynamic,--no-whole-archive,-lcryptopp'
    else:
        if program.env.DEST_BINFMT == 'elf':
            # All ELF platforms are impacted but only the gcc compiler has a flag to fix it.
            if 'gcc' in (program.env.CXX_NAME, program.env.CC_NAME): 
                program.env.append_value ('SHLIB_MARKER', '-Wl,--no-as-needed,-lcryptopp')

    return program
```
search
```python
obj.install_path = None
```
and add 
```python
obj.uselib = ‘CRYPTOPP'
```
after the two results, which makes it looks like:
```python
            if os.path.isdir(os.path.join("scratch", filename)):
                obj = bld.create_ns3_program(filename, all_modules)
                obj.path = obj.path.find_dir('scratch').find_dir(filename)
                obj.source = obj.path.ant_glob('*.cc')
                obj.target = filename
                obj.name = obj.target
                obj.install_path = None
                obj.uselib = 'CRYPTOPP' 
            elif filename.endswith(".cc"):
                name = filename[:-len(".cc")]
                obj = bld.create_ns3_program(name, all_modules)
                obj.path = obj.path.find_dir('scratch')
                obj.source = filename
                obj.target = name
                obj.name = obj.target
                obj.install_path = None
                obj.uselib = 'CRYPTOPP' 
```

### Change config for new source file
open the wscript file in applications and brite folder, add the name of those .cc files and .h files that your copied in step 3. 
```python
    module.source = [
        'model/bulk-send-application.cc',
        'model/onoff-application.cc',
        'model/packet-sink.cc',
        'model/udp-client.cc',
        'model/udp-server.cc',
        'model/seq-ts-header.cc',
        'model/udp-trace-client.cc',
        'model/packet-loss-counter.cc',
        'model/udp-echo-client.cc',
        'model/udp-echo-server.cc',
        'model/application-packet-probe.cc',
        'model/three-gpp-http-client.cc',
        'model/three-gpp-http-server.cc',
        'model/three-gpp-http-header.cc',
        'model/three-gpp-http-variables.cc', 
        'model/BlockChainApplicationBase.cc',
        'model/PBFTCorrect.cc',
        'model/ConsensusMessage.cc',
        'model/PBFTMessage.cc',
        'model/MessageRecvPool.cc',
        'helper/bulk-send-helper.cc',
        'helper/on-off-helper.cc',
        'helper/packet-sink-helper.cc',
        'helper/udp-client-server-helper.cc',
        'helper/udp-echo-helper.cc',
        'helper/three-gpp-http-helper.cc',
        'helper/BlockChainBaseHelper.cc',
        'helper/PBFTCorrectHelper.cc',
        'helper/BlockChainTopologyHelper.cc',
        'helper/GeoSimulationTopologyHelper.cc',
        ]
```
```python
    headers.source = [
        'model/bulk-send-application.h',
        'model/onoff-application.h',
        'model/packet-sink.h',
        'model/udp-client.h',
        'model/udp-server.h',
        'model/seq-ts-header.h',
        'model/udp-trace-client.h',
        'model/packet-loss-counter.h',
        'model/udp-echo-client.h',
        'model/udp-echo-server.h',
        'model/application-packet-probe.h',
        'model/three-gpp-http-client.h',
        'model/three-gpp-http-server.h',
        'model/three-gpp-http-header.h',
        'model/three-gpp-http-variables.h',
        'model/BlockChainApplicationBase.h',
        'model/PBFTCorrect.h',
        'model/ConsensusMessage.h',
        'model/PBFTMessage.h',
        'model/MessageRecvPool.h',
        'helper/bulk-send-helper.h',
        'helper/on-off-helper.h',
        'helper/packet-sink-helper.h',
        'helper/udp-client-server-helper.h',
        'helper/udp-echo-helper.h',
        'helper/three-gpp-http-helper.h',
        'helper/BlockChainBaseHelper.h',
        'helper/PBFTCorrectHelper.h',
        'helper/BlockChainTopologyHelper.h',
        'helper/GeoSimulationTopologyHelper.h',
        ]
```
```python
    if bld.env['ENABLE_BRITE']:
        module.source.append ('helper/brite-topology-helper.cc')
        module.source.append ('helper/myBriteTopologyHelper.cc')
        headers.source.append ('helper/brite-topology-helper.h')
        headers.source.append ('helper/myBriteTopologyHelper.h')
        module_test.source.append('test/brite-test-topology.cc')
```
### build
```shell
sudo ./waf distclean
sudo ./waf configure --with-brite=/your/path/to/brite/source [other build flags you want]
sudo ./waf
```
