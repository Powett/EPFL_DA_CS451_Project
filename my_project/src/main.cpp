#include <chrono>
#include <iostream>
#include <semaphore.h>
#include <thread>
#include <signal.h>

#include "messaging.hpp"
#include "parser.hpp"
#include "pendinglist.hpp"
#include "defines.hpp"


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
#ifdef DEBUG_MODE
  cout << "Stopping network packet processing.\n";
#endif
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
#ifdef DEBUG_MODE
  cout << "Closing logfile.\n";
#endif
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

#ifdef DEBUG_MODE
  cout << "My PID: " << getpid() << "\n";
  cout << "From a new terminal type `kill -SIGINT " << getpid()
       << "` or `kill -SIGTERM " << getpid()
       << "` to stop processing packets\n\n";
#endif

#ifdef DEBUG_MODE
  cout << "My ID: " << parser.id() << "\n\n";
#endif

  // Parse config file
  Parser::PerfectLinkConfig vals = parser.perfectLinkValues();
#ifdef DEBUG_MODE
  cout << "Perfect Link config:" << endl;
  cout << "==========================\n";
  cout << vals.nb_messages << " messages to be sent to " << vals.rID << endl;
  cout << endl;
#endif

// Parse hosts file
#ifdef DEBUG_MODE
  cout << "List of resolved hosts is:\n";
  cout << "==========================\n";
#endif
  auto hosts = parser.hosts();
  Parser::Host *self_host = NULL;
  Parser::Host *dest_host = NULL;

  for (auto &host : hosts) {
#ifdef DEBUG_MODE
    cout << host.id << " : ";
    cout << host.ipReadable() << ":" << host.portReadable() << endl;
    cout << "Machine: " << host.ip << ":" << host.port << endl;
#endif
    if (host.id == parser.id()) {
      self_host = &host;
#ifdef DEBUG_MODE
      cout << " [self]";
#endif
    }
    if (host.id == vals.rID) {
      dest_host = &host;
#ifdef DEBUG_MODE
      cout << " [dest]";
#endif
    }
#ifdef DEBUG_MODE
    cout << endl;
#endif
  }
#ifdef DEBUG_MODE
  cout << endl;
#endif

#ifdef DEBUG_MODE
  cout << "Path to output:\n";
  cout << "===============\n";
  cout << parser.outputPath() << "\n\n";
#endif

#ifdef DEBUG_MODE
  cout << "Path to config:\n";
  cout << "===============\n";
  cout << parser.configPath() << "\n\n";
  cout << "===============\n";
#endif

// Create UDP socket
#ifdef DEBUG_MODE
  cout << "Creating socket on " << self_host->ipReadable() << ":"
       << self_host->portReadable() << endl;
#endif
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
    for (int i = 1; i <= vals.nb_messages; i++) {
      message *current =
          new message{dest_host, to_string(i), to_string(i).length() + 1};
      pending.push_last(current);
      logFile << "b " << current->msg << std::endl;
    }
  }

#ifdef DEBUG_MODE
  cout << "Message list:\n" << pending;
#endif

  // Allow logging for receivers (effectively starting listeners)
  sem_post(&logSem);

  // Start sender(s)
  for (int i = 0; i < NSENDERS; i++) {
    senderThreads[i] = thread(&UDPSocket::sender, sock, std::ref(pending),
                              std::ref(hosts), &stop_threads);
  }

#ifdef DEBUG_MODE
  cout << "Broadcasting and delivering messages...\n\n";
#endif

  // After a process finishes broadcasting,
  // it waits forever for the delivery of messages.
  while (true) {
    this_thread::sleep_for(chrono::hours(1));
  }

  return 0;
}
