#include "src/pir.hpp"
#include "src/pir_client.hpp"
#include "src/pir_server.hpp"
#include <seal/seal.h>
#include <chrono>
#include <memory>
#include <random>
#include <cstdint>
#include <cstddef>
#include <grpcpp/grpcpp.h>
#include "proto/payload.grpc.pb.h"

using namespace std::chrono;
using namespace std;
using namespace seal;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using pir::Request;
using pir::Response;
using pir::Request1;
using pir::Response1;
using pir::Query;
using pir::KeyRequest;
using pir::KeyResponse;
using pir::HelloReply;
using pir::HelloRequest;


std::map<int, std::string> key_map = {
  {0, "111111"},
};

std::stringstream deal_response(std::stringstream &in);

// Logic and data behind the server's behavior.
class PIRServiceImpl final : public Query::Service {

 // Hello imple just for test.
  grpc::Status sayHello(ServerContext* context, const HelloRequest* request,
                  HelloReply* reply) override {
    std::cout << "server register sayHello service.. " << std::endl;
    std::string prefix("Hello ");
    reply->set_message(prefix + request->name());
    std::cout << "server register sayHello service init finished.. " << std::endl;

    return grpc::Status::OK;
  }

  grpc::Status sendQueryStream(ServerContext* context, const Request1* request,
                 Response1* reply) override {
    std::cout << "server ProcessRequest begin."  << std::endl;
    std::stringstream in(request->query());
    std::stringstream out = deal_response(in);
    std::cout << "server ProcessRequest finish."  << std::endl;
    
    reply->set_reply(out.str());
    return grpc::Status::OK;
  }

  grpc::Status sendGalorisKey(ServerContext* context, const KeyRequest* request,
                 KeyResponse* reply) override {
    std::cout << "enter server gal grpc" << std::endl;
    
    key_map[0] = request->galois_keys();
    std::string prefix("ok");
    reply->set_reply(prefix);
    std::cout << "server ProcessGaloriskey finish."  << std::endl;
    return grpc::Status::OK;
  }
};

void RunServer() {
  std::string server_address("0.0.0.0:50051");
  PIRServiceImpl service;

  grpc::EnableDefaultHealthCheckService(true);
  // grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

std::stringstream  deal_response(std::stringstream &client_stream) {

    uint64_t number_of_items = 1 << 10;
    uint64_t size_per_item = 128; // in bytes
    uint32_t N = 4096;

    // Recommended values: (logt, d) = (20, 2).
    uint32_t logt = 20;
    uint32_t d = 2;
    bool use_symmetric = true; // use symmetric encryption instead of public key (recommended for smaller query)
    bool use_batching = true; // pack as many elements as possible into a BFV plaintext (recommended)
    bool use_recursive_mod_switching = true;

    EncryptionParameters enc_params(scheme_type::bfv);
    PirParams pir_params;

    // Generates all parameters
    
    cout << "Main: Generating SEAL parameters" << endl;
    gen_encryption_params(N, logt, enc_params);
    
    cout << "Main: Verifying SEAL parameters" << endl;
    verify_encryption_params(enc_params);
    cout << "Main: SEAL parameters are good" << endl;

    cout << "Main: Generating PIR parameters" << endl;
    gen_pir_params(number_of_items, size_per_item, d, enc_params, pir_params, use_symmetric, use_batching, use_recursive_mod_switching);
    
    
    print_seal_params(enc_params); 
    print_pir_params(pir_params);


    // Initialize PIR Server
    cout << "Main: Initializing server" << endl;
    PIRServer server(enc_params, pir_params);
    std::cout << "test1" << std::endl;

    // Server maps the galois key to client 0. We only have 1 client,
    // which is why we associate it with 0. If there are multiple PIR
    // clients, you should have each client generate a galois key, 
    // and assign each client an index or id, then call the procedure below.
    
    std::istringstream galkey(key_map[0].c_str());
    // seal::GaloisKeys galois_keys = server.load_galois_key(galkey);
    
    std::cout << "test1" << std::endl;

    // server.set_galois_key(0, galois_keys);

    cout << "Main: Creating the database with random data (this may take some time) ..." << endl;

    // Create test database
    auto db(make_unique<uint8_t[]>(number_of_items * size_per_item));



    random_device rd;
    for (uint64_t i = 0; i < number_of_items; i++) {
        for (uint64_t j = 0; j < size_per_item; j++) {
            uint8_t val = rd() % 256;
            db.get()[(i * size_per_item) + j] = val;
        }
    }

    // Measure database setup
    auto time_pre_s = high_resolution_clock::now(); 
    server.set_database(move(db), number_of_items, size_per_item);
    server.preprocess_database();
    auto time_pre_e = high_resolution_clock::now();
    auto time_pre_us = duration_cast<microseconds>(time_pre_e - time_pre_s).count();
    cout << "Main: database pre processed " << endl;




    // Measure serialized query generation (useful for sending over the network)
    stringstream server_stream;

    // Measure query deserialization (useful for receiving over the network)
    auto time_deserial_s = high_resolution_clock::now();
    PirQuery query2 = server.deserialize_query(client_stream);
    auto time_deserial_e = high_resolution_clock::now();
    auto time_deserial_us = duration_cast<microseconds>(time_deserial_e - time_deserial_s).count();
    cout << "Main: query deserialized" << endl;

    // Measure query processing (including expansion)
    auto time_server_s = high_resolution_clock::now();
    // Answer PIR query from client 0. If there are multiple clients, 
    // enter the id of the client (to use the associated galois key).
    PirReply reply = server.generate_reply(query2, 0); 
    auto time_server_e = high_resolution_clock::now();
    auto time_server_us = duration_cast<microseconds>(time_server_e - time_server_s).count();
    cout << "Main: reply generated" << endl;

    // Serialize reply (useful for sending over the network)
    int reply_size = server.serialize_reply(reply, server_stream);


    // Output results
    cout << "Main: PIR result correct!" << endl;
    cout << "Main: PIRServer pre-processing time: " << time_pre_us / 1000 << " ms" << endl;
    // cout << "Main: PIRClient query generation time: " << time_query_us / 1000 << " ms" << endl;
    // cout << "Main: PIRClient serialized query generation time: " << time_s_query_us / 1000 << " ms" << endl;
    cout << "Main: PIRServer query deserialization time: " << time_deserial_us << " us" << endl;
    cout << "Main: PIRServer reply generation time: " << time_server_us / 1000 << " ms" << endl;
    // cout << "Main: PIRClient answer decode time: " << time_decode_us / 1000 << " ms" << endl;
    // cout << "Main: Query size: " << query_size << " bytes" << endl;
    cout << "Main: Reply num ciphertexts: " << reply.size() << endl;
    cout << "Main: Reply size: " << reply_size << " bytes" << endl;

    return server_stream;
}

int main(int argc, char *argv[]) {
  RunServer();
  return 0;
}
