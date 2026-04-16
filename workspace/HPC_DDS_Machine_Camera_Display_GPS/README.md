# Config dds-cxx-2.5.0-1 vo sdk para-sdk_raspi-raspios64_bookworm

sed -i 's/~Reference<DELEGATE>()/~Reference()/g' /opt/para-sdk_raspi-raspios64_bookworm/external/include/ddscxx/dds/core/detail/ReferenceImpl.hpp

sed -i 's/~Topic<T>()/~Topic()/g' /opt/para-sdk_raspi-raspios64_bookworm/external/include/ddscxx/dds/topic/detail/TTopicImpl.hpp

sed -i 's/~DataReader<T>()/~DataReader()/g' /opt/para-sdk_raspi-raspios64_bookworm/external/include/ddscxx/dds/sub/detail/TDataReaderImpl.hpp

sed -i 's/~DataWriter<T>()/~DataWriter()/g' /opt/para-sdk_raspi-raspios64_bookworm/external/include/ddscxx/dds/pub/detail/DataWriterImpl.hpp


# Roudi fix (install/etc/ipc/roudi_config.toml)
```
[general]
version = 1
 
[[segment]]
 
[[segment.mempool]]
size = 128
count = 10000

[[segment.mempool]]
size = 1024
count = 5000

[[segment.mempool]]
size = 16384
count = 1000

[[segment.mempool]]
size = 131072
count = 200

[[segment.mempool]]
size = 524288
count = 50

[[segment.mempool]]
size = 1048576
count = 30

[[segment.mempool]]
size = 4194304
count = 10

[[segment.mempool]]
size = 33554432
count = 2

[[segment.mempool]]
size = 134217728
count = 2

```