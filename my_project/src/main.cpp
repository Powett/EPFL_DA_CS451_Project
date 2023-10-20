#include <chrono>
#include <iostream>
#include <semaphore.h>
#include <thread>

#include "messaging.hpp"
#include "parser.hpp"
#include "pendinglist.hpp"
#include <signal.h>

#define NLISTENERS 4
#define NSENDERS 3

using namespace std;

UDPSocket *sock;

ofstream logFile;
sem_t logSem;
thread listenerThreads[NLISTENERS];
thread senderThreads[NSENDERS];

PendingList pending;

bool stop_threads = false;

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  // immediately stop network packet processing
  // DEBUG
  cout << "Stopping network packet processing.\n";
  // ENDDEBUG
  delete sock;

  // kill all threads ?
  stop_threads = true;
  for (int i = 0; i < NLISTENERS; i++) {
    (listenerThreads[i]).join();
  }
  for (int i = 0; i < NSENDERS; i++) {
    (senderThreads[i]).join();
  }

  // Clean pending: automatic destructor

  // Closing logFile
  // DEBUG
  cout << "Closing logfile.\n";
  // ENDDEBUG
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

  // DEBUG
  cout << "My PID: " << getpid() << "\n";
  cout << "From a new terminal type `kill -SIGINT " << getpid()
       << "` or `kill -SIGTERM " << getpid()
       << "` to stop processing packets\n\n";
  // ENDDEBUG

  // DEBUG
  cout << "My ID: " << parser.id() << "\n\n";
  // ENDDEBUG

  // Parse config file
  Parser::PerfectLinkConfig vals = parser.perfectLinkValues();
  // DEBUG
  cout << "Perfect Link config:" << endl;
  cout << "==========================\n";
  cout << vals.nb_messages << " messages to be sent to " << vals.rID << endl;
  cout << endl;
  // ENDDEBUG

  // Parse hosts file
  // DEBUG
  cout << "List of resolved hosts is:\n";
  cout << "==========================\n";
  // ENDDEBUG
  auto hosts = parser.hosts();
  Parser::Host *self_host = NULL;
  Parser::Host *dest_host = NULL;

  for (auto &host : hosts) {
    // DEBUG
    cout << host.id << " : ";
    cout << host.ipReadable() << ":" << host.portReadable() << endl;
    cout << "Machine: " << host.ip << ":" << host.port << endl;
    // ENDDEBUG
    if (host.id == parser.id()) {
      self_host = &host;
      // DEBUG
      cout << " [self]";
      // ENDDEBUG
    }
    if (host.id == vals.rID) {
      dest_host = &host;
      // DEBUG
      cout << " [dest]";
      // ENDDEBUG
    }
    // DEBUG
    cout << endl;
    // ENDDEBUG
  }
  // DEBUG
  cout << endl;
  // ENDDEBUG

  // DEBUG
  cout << "Path to output:\n";
  cout << "===============\n";
  cout << parser.outputPath() << "\n\n";
  // ENDDEBUG

  // DEBUG
  cout << "Path to config:\n";
  cout << "===============\n";
  cout << parser.configPath() << "\n\n";
  cout << "===============\n";
  // ENDDEBUG

  // Create UDP socket
  // DEBUG
  cout << "Creating socket on " << self_host->ipReadable() << ":"
       << self_host->portReadable() << endl;
  // ENDDEBUG
  sock = new UDPSocket(self_host->ip, self_host->port);

  // Open logfile
  logFile.open(parser.outputPath());

  // Start listener(s)
  for (int i = 0; i < NLISTENERS; i++) {
    listenerThreads[i] =
        thread(&UDPSocket::listener, sock, std::ref(pending), &logFile, &logSem,
               std::ref(hosts), &stop_threads);
  }

  // Build message queue
  if (self_host != dest_host) {
    for (int i = vals.nb_messages; i > 0; i--) {
      message *current = new message{dest_host, to_string(i), 2, nullptr};
      pending.push(current);
      logFile << "b " << current->msg << std::endl;
    }
  }

  // Allow logging for receivers
  sem_post(&logSem);

  // Send messages
  for (int i = 0; i < NSENDERS; i++) {
    senderThreads[i] = thread(&UDPSocket::sender, sock, std::ref(pending),
                              std::ref(hosts), &stop_threads);
  }

  // DEBUG
  cout << "Broadcasting and delivering messages...\n\n";
  // ENDDEBUG

  // After a process finishes broadcasting,
  // it waits forever for the delivery of messages.
  while (true) {
    this_thread::sleep_for(chrono::hours(1));
  }

  return 0;
}
