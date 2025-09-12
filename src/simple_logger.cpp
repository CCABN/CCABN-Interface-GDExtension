#include "simple_logger.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void SimpleLogger::_bind_methods() {
}

SimpleLogger::SimpleLogger() {
}

SimpleLogger::~SimpleLogger() {
}

void SimpleLogger::_enter_tree() {
	print_line("CCABN VideoStream GDExtension: SimpleLogger node entered the tree!");
}

void SimpleLogger::_ready() {
	print_line("CCABN VideoStream GDExtension: SimpleLogger node is ready!");
}