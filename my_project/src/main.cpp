#include <chrono>
#include <iostream>
#include <semaphore.h>
#include <thread>

#include "messaging.hpp"
#include "parser.hpp"
#include <signal.h>

#define NLISTENERS 3
#define NSENDERS 3

using namespace std;

ofstream logFile;
sem_t logSem, pendSem;
thread listenerThreads[NLISTENERS];
thread senderThreads[NSENDERS];

messageList pending = NULL;

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  // immediately stop network packet processing
  cout << "Immediately stopping network packet processing.\n";

  // kill all threads ?

  // Clean pending
  cleanup(pending);

  // write/flush output file if necessary
  cout << "Writing output.\n";

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

  cout << "My PID: " << getpid() << "\n";
  cout << "From a new terminal type `kill -SIGINT " << getpid()
       << "` or `kill -SIGTERM " << getpid()
       << "` to stop processing packets\n\n";

  cout << "My ID: " << parser.id() << "\n\n";

  // Parse config file
  Parser::PerfectLinkConfig vals = parser.perfectLinkValues();
  cout << "Perfect Link config:" << endl;
  cout << "==========================\n";
  cout << vals.nb_messages << " messages to be sent to " << vals.rID << endl;
  cout << endl;

  // Parse hosts file
  cout << "List of resolved hosts is:\n";
  cout << "==========================\n";
  auto hosts = parser.hosts();
  Parser::Host *self_host = NULL;
  Parser::Host *dest_host = NULL;

  for (auto &host : hosts) {
    cout << host.id << " : ";
    cout << host.ipReadable() << ":" << host.portReadable();
    // cout << "Machine: " << host.ip << ":" << host.port;
    if (host.id == parser.id()) {
      self_host = &host;
      cout << " [self]";
    }
    if (host.id == vals.rID) {
      dest_host = &host;
      cout << " [dest]";
    }
    cout << endl;
  }
  cout << endl;

  cout << "Path to output:\n";
  cout << "===============\n";
  cout << parser.outputPath() << "\n\n";

  cout << "Path to config:\n";
  cout << "===============\n";
  cout << parser.configPath() << "\n\n";
  cout << "===============\n";

  // Create UDP socket
  cout << "Creating socket on " << self_host->ipReadable() << ":"
       << self_host->portReadable() << endl;
  UDPSocket sock = UDPSocket(self_host->ip, self_host->port);

  // Open logfile
  logFile.open(parser.outputPath());

  // Start listener(s)
  for (int i = 0; i < NLISTENERS; i++) {
    listenerThreads[i] =
        thread(&UDPSocket::listener, &sock, &logFile, &logSem, hosts);
  }
  sem_post(&logSem);

  // Build message queue
  if (self_host != dest_host) {
    for (int i = 0; i < vals.nb_messages; i++) {
      pending = new message{dest_host, to_string(i), 2, pending};
    }
  }

  // Send messages
  for (int i = 0; i < NSENDERS; i++) {
    senderThreads[i] = thread(&UDPSocket::sender, &sock, &pending, &pendSem,
                              &logFile, &logSem, hosts);
  }
  sem_post(&pendSem);

  cout << "Broadcasting and delivering messages...\n\n";

  // After a process finishes broadcasting,
  // it waits forever for the delivery of messages.
  while (true) {
    this_thread::sleep_for(chrono::hours(1));
  }

  return 0;
}
