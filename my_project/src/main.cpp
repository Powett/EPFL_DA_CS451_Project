#include <atomic>
#include <chrono>
#include <iostream>

#include <signal.h>
#include <thread>
#include <unordered_set>

#include "defines.hpp"
#include "lattice.hpp"
#include "messaging.hpp"
#include "node.hpp"
#include "parser.hpp"
#include "pendinglist.hpp"

using namespace std;

thread listenerThread;
thread senderThread;

Node *node;

static void stop(int) {
  // set default handlers
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  // immediately stop network packet processing
#if DEBUG_MODE > 0
  cout << "Stopping network packet processing.\n";
#endif

  // kill all threads
  node->stopThreads = true;

  if (senderThread.joinable()) {
    senderThread.join();
  }

  if (listenerThread.joinable()) {
    listenerThread.join();
  }

  // log messages
  node->logDecision();

  // clean hosts
  for (auto &host : node->hosts) {
    delete host;
  }

// Closing logFile
#if DEBUG_MODE > 0
  cout << "Closing logfile.\n";
#endif
  node->logFile.close();

  // Notify the exit
  std::cout << "Exited from signal, sending queue was "
            << (node->pending.empty() ? "" : "not") << " empty" << endl;

  // cleanup
  delete node;

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
  node = new Node();

#if DEBUG_MODE > 1
  cout << "My PID: " << getpid() << "\n";
  cout << "From a new terminal type `kill -SIGINT " << getpid()
       << "` or `kill -SIGTERM " << getpid()
       << "` to stop processing packets\n\n";
#endif

#if DEBUG_MODE > 1
  cout << "My ID: " << parser.id() << "\n\n";
#endif

  // Parse config file
  Parser::LAConfig const &la_vals = parser.laConfigValues();

#if DEBUG_MODE > 1

  cout << "LA config:" << endl;
  cout << "==========================\n";
  cout << "p vs ds: " << la_vals.p << " " << la_vals.vs << " " << la_vals.ds
       << endl;
  cout << "Proposed values:" << endl;
  for (auto const &v : la_vals.proposed_values) {
    cout << "[" << lavals_to_string(v) << "]" << endl;
  }
  cout << endl;
#endif

// Parse hosts file
#if DEBUG_MODE > 1
  cout << "List of resolved hosts is:\n";
  cout << "==========================\n";
#endif
  node->hosts = std::vector<Parser::Host *>(parser.hosts());
  node->id = parser.id();

  for (auto &host : node->hosts) {
#if DEBUG_MODE > 0
    cout << host->id << " : ";
    cout << host->ipReadable() << ":" << host->portReadable() << endl;
    cout << "Machine: " << host->ip << ":" << host->port << endl;
#endif
    if (host->id == node->id) {
      node->self_host = host;
#if DEBUG_MODE > 1
      cout << " [self]";
#endif
    }
#if DEBUG_MODE > 1
    cout << endl;
#endif
  }
#if DEBUG_MODE > 1
  cout << endl;
#endif

#if DEBUG_MODE > 1
  cout << "Path to output:\n";
  cout << "===============\n";
  cout << parser.outputPath() << "\n\n";
#endif

#if DEBUG_MODE > 1
  cout << "Path to config:\n";
  cout << "===============\n";
  cout << parser.configPath() << "\n\n";
  cout << "===============\n";
#endif

// Create UDP socket
#if DEBUG_MODE > 1
  cout << "Creating socket on " << node->self_host->ipReadable() << ":"
       << node->self_host->portReadable() << endl;
#endif
  UDPSocket sock = UDPSocket(node->self_host->ip, node->self_host->port);
  node->sock = &sock;
  // Open logfile
  node->logFile.open(parser.outputPath());

  // #if DEBUG_MODE > 0
  //   cout << "Message list:\n" << node->pending << endl;
  // #endif

  // Start listener(s)
  listenerThread = thread(&Node::bebListener, node);

  // Start sender(s)
  senderThread = thread(&Node::bebSender, node);

  for (size_t i(0); i < la_vals.p; i++) {
    // propose new value
    std::string uniqID = std::to_string(node->id) + "/" + std::to_string(i+1);
    node->bebBroadcast(LAMessage(PROPOSE, 0, i+1, la_vals.proposed_values[i]),
                       uniqID);
    // usleep(WAIT_US);
  }

#if DEBUG_MODE > 0
  cout << "Broadcasting and delivering messages...\n\n";
#endif

  // After a process finishes broadcasting,
  // it waits forever for the delivery of messages.
  while (true) {
    this_thread::sleep_for(chrono::hours(1));
  }

  return 0;
}
