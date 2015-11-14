# based on: http://code.google.com/p/ostinato/source/browse/protobuf.pri
#
# qt qmake integration with google protocol buffers compiler protoc
#
# to compile protocol buffers with qt qmake, specify protos variable and
# include this file
#
# example:
# protos = a.proto b.proto
# include(protobuf.pri)
#
# set proto_path if you need to set the protoc --proto_path search path
# set protoc to the path to the protoc compiler if it is not in your $path
#

isempty(proto_dir):proto_dir = .
isempty(protoc):protoc = protoc

protopaths =
for(p, proto_path):protopaths += --proto_path=$${p}

protobuf_decl.name  = protobuf header
protobuf_decl.input = protos
protobuf_decl.output  = $${proto_dir}/${qmake_file_base}.pb.h
protobuf_decl.commands = $${protoc} --cpp_out="$${proto_dir}" $${protopaths} --proto_path=${qmake_file_in_path} ${qmake_file_name}
protobuf_decl.variable_out = generated_files
qmake_extra_compilers += protobuf_decl

protobuf_impl.name  = protobuf implementation
protobuf_impl.input = protos
protobuf_impl.output  = $${proto_dir}/${qmake_file_base}.pb.cc
protobuf_impl.depends  = $${proto_dir}/${qmake_file_base}.pb.h
protobuf_impl.commands = $$escape_expand(\\n)
protobuf_impl.variable_out = generated_sources
qmake_extra_compilers += protobuf_impl
