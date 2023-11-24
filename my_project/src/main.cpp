#include <atomic>
#include <chrono>
#include <iostream>

#include <signal.h>
#include <thread>
#include <unordered_set>

#include "defines.hpp"
#include "messaging.hpp"
#include "node.hpp"
#include "parser.hpp"
#include "pendinglist.hpp"

using namespace std;

thread listenerThread;
thread senderThread;
thread messageManagerThread;

Node *node;

static void stop(int) {
  // set default handlers
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  // immediately stop network packet processing
#ifdef DEBUG_MODE
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

  if (messageManagerThread.joinable()) {
    messageManagerThread.join();
  }

  // log last messages
  node->tryDeliver();

  // clean hosts
  for (auto &host : node->hosts) {
    delete host;
  }

// Closing logFile
#ifdef DEBUG_MODE
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
  Parser::FIFOBroadcastConfig fb_vals = {0};
  fb_vals = parser.fifoBroadcastValues();
#ifdef DEBUG_MODE

  cout << "FIFO Broadcast config:" << endl;
  cout << "==========================\n";
  cout << fb_vals.nb_messages << " messages to be sent " << endl;

  cout << endl;
#endif

// Parse hosts file
#ifdef DEBUG_MODE
  cout << "List of resolved hosts is:\n";
  cout << "==========================\n";
#endif
  node->hosts = std::vector<Parser::Host *>(parser.hosts());
  node->id = parser.id();

  for (auto &host : node->hosts) {
#ifdef DEBUG_MODE
    cout << host->id << " : ";
    cout << host->ipReadable() << ":" << host->portReadable() << endl;
    cout << "Machine: " << host->ip << ":" << host->port << endl;
#endif
    if (host->id == node->id) {
      node->self_host = host;
#ifdef DEBUG_MODE
      cout << " [self]";
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
  cout << "Creating socket on " << node->self_host->ipReadable() << ":"
       << node->self_host->portReadable() << endl;
#endif
  UDPSocket sock = UDPSocket(node->self_host->ip, node->self_host->port);
  node->sock = &sock;
  // Open logfile
  node->logFile.open(parser.outputPath());

  // Build message queue
  // Unsafe struct to store all messages to be sent eventually
  std::vector<std::string> messages;
  for (int i = 1; i <= fb_vals.nb_messages; i++) {
    messages.push_back(to_string(i));
    node->logFile << "b " << to_string(i) << std::endl;
  }

#ifdef DEBUG_MODE
  cout << "Message list:\n" << node->pending << endl;
#endif

  // Start listener(s)

  listenerThread = thread(&Node::bebListener, node);

  // Start sender(s)

  senderThread = thread(&Node::bebSender, node);

  // Start failure detector

  messageManagerThread =
      thread(&Node::messageManager, node, std::ref(messages));

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
