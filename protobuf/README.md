## Compiling ProtoBuf's definitions

### Installing the compiler

Install protoc using the instructions [here](https://grpc.io/docs/protoc-installation/).

### Compiling

To compile the protocol buffer definitions and generate the classes, run

```shell
$ protoc -I=./ --cpp_out=./ ./cell_polygons.proto
```