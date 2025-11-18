#include "event_queue.h"

using namespace framework;

std::unordered_map<std::type_index, EventQueue::TypeOps> EventQueue::s_mapRegistry{};
