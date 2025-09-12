#ifndef SIMPLE_LOGGER_H
#define SIMPLE_LOGGER_H

#include <godot_cpp/classes/node.hpp>

using namespace godot;

class SimpleLogger : public Node {
	GDCLASS(SimpleLogger, Node)

protected:
	static void _bind_methods();

public:
	SimpleLogger();
	~SimpleLogger();

	void _enter_tree() override;
	void _ready() override;
};

#endif