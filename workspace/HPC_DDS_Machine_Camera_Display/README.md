# Config dds-cxx-2.5.0-1 vo sdk para-sdk_raspi-raspios64_bookworm

sed -i 's/~Reference<DELEGATE>()/~Reference()/g' /opt/para-sdk_raspi-raspios64_bookworm/external/include/ddscxx/dds/core/detail/ReferenceImpl.hpp

sed -i 's/~Topic<T>()/~Topic()/g' /opt/para-sdk_raspi-raspios64_bookworm/external/include/ddscxx/dds/topic/detail/TTopicImpl.hpp

sed -i 's/~DataReader<T>()/~DataReader()/g' /opt/para-sdk_raspi-raspios64_bookworm/external/include/ddscxx/dds/sub/detail/TDataReaderImpl.hpp

sed -i 's/~DataWriter<T>()/~DataWriter()/g' /opt/para-sdk_raspi-raspios64_bookworm/external/include/ddscxx/dds/pub/detail/DataWriterImpl.hpp


# DDS-Camera-Display Project