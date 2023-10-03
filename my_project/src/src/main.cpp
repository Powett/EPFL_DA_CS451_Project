#include <chrono>
#include <iostream>
#include <thread>
#include <semaphore.h>

#include "hello.h"
#include "messaging.hpp"
#include "parser.hpp"
#include <signal.h>

#define NLISTENERS 3
#define NSENDERS 3

std::ofstream logFile;
sem_t logSem;
std::thread listenerThreads[NLISTENERS];
std::thread senderThreads[NSENDERS]; 

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  // immediately stop network packet processing
  std::cout << "Immediately stopping network packet processing.\n";

  // write/flush output file if necessary
  std::cout << "Writing output.\n";

  // Closing logFile
  logFile.close();

  // exit directly from signal handler
  exit(0);
}

int main(int argc, char **argv) {
  signal(SIGTERM, stop);
  signal(SIGINT, stop);

  // `true` means that a config file is required.
  // Call with `false` if no config file is necessary.
  bool requireConfig = true;

  Parser parser(argc, argv);
  parser.parse();

  hello();
  std::cout << std::endl;

  std::cout << "My PID: " << getpid() << "\n";
  std::cout << "From a new terminal type `kill -SIGINT " << getpid()
            << "` or `kill -SIGTERM " << getpid()
            << "` to stop processing packets\n\n";

  std::cout << "My ID: " << parser.id() << "\n\n";

  std::cout << "List of resolved hosts is:\n";
  std::cout << "==========================\n";
  auto hosts = parser.hosts();
  Parser::Host *self_host=NULL;
  for (auto &host : hosts) {
    std::cout << host.id << "\n";
    std::cout << "Human-readable IP: " << host.ipReadable() << "\n";
    std::cout << "Machine-readable IP: " << host.ip << "\n";
    std::cout << "Human-readable Port: " << host.portReadable() << "\n";
    std::cout << "Machine-readable Port: " << host.port << "\n";
    std::cout << "\n";
    if (host.id == parser.id()){
      self_host=&host;
    }
  }
  std::cout << "\n";

  std::cout << "Path to output:\n";
  std::cout << "===============\n";
  std::cout << parser.outputPath() << "\n\n";

  std::cout << "Path to config:\n";
  std::cout << "===============\n";
  std::cout << parser.configPath() << "\n\n";
  std::cout << "===============\n";

  // Parse config file
  auto vals = parser.perfectLinkValues();
  std::cout << "Perfect Link config:" << std::endl;
  std::cout << vals.nb_messages << " messages to be sent to " << vals.rID
            << std::endl;

  // Create UDP socket
  std::cout << "Creating socket on " << self_host->ipReadable() << ":" << self_host->portReadable() << std::endl;
  UDPSocket sock = UDPSocket(self_host->ip, self_host->port);

  // Open logfile
  logFile.open(parser.outputPath());

  // Start listener(s)
  for (int i=0;i<NLISTENERS;i++){
    listenerThreads[i]=std::thread(&UDPSocket::listener, &sock, &logFile, &logSem);
  }
  // Send messages



  std::cout << "Broadcasting and delivering messages...\n\n";

  // After a process finishes broadcasting,
  // it waits forever for the delivery of messages.
  while (true) {
    std::this_thread::sleep_for(std::chrono::hours(1));
  }

  return 0;
}
