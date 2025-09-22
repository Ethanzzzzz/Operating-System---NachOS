#include <pthread.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include "consumer.hpp"
#include "ts_queue.hpp"
#include "item.hpp"
#include "transformer.hpp"

#ifndef CONSUMER_CONTROLLER
#define CONSUMER_CONTROLLER

class ConsumerController : public Thread {
public:
	// constructor
	ConsumerController(
		TSQueue<Item*>* worker_queue,
		TSQueue<Item*>* writer_queue,
		Transformer* transformer,
		int check_period,
		int low_threshold,
		int high_threshold
	);

	// destructor
	~ConsumerController();

	virtual void start();

private:
	std::vector<Consumer*> consumers;

	TSQueue<Item*>* worker_queue;
	TSQueue<Item*>* writer_queue;

	Transformer* transformer;

	// Check to scale down or scale up every check period in microseconds.
	int check_period;
	// When the number of items in the worker queue is lower than low_threshold,
	// the number of consumers scaled down by 1.
	int low_threshold;
	// When the number of items in the worker queue is higher than high_threshold,
	// the number of consumers scaled up by 1.
	int high_threshold;

	static void* process(void* arg);
};

// Implementation start

ConsumerController::ConsumerController(
	TSQueue<Item*>* worker_queue,
	TSQueue<Item*>* writer_queue,
	Transformer* transformer,
	int check_period,
	int low_threshold,
	int high_threshold
) : worker_queue(worker_queue),
	writer_queue(writer_queue),
	transformer(transformer),
	check_period(check_period),
	low_threshold(low_threshold),
	high_threshold(high_threshold) {
}

ConsumerController::~ConsumerController() {}

void ConsumerController::start() {
	// TODO: starts a ConsumerController thread
	pthread_create(&t, nullptr, ConsumerController::process, (void *) this);
}

void* ConsumerController::process(void* arg) {
	// TODO: implements the ConsumerController's work
	ConsumerController *ctr = (ConsumerController*) arg;

	// check every check_period microseconds
	// std::cout << "Consumer Controller processing" << std::endl;
	while(true){
		usleep(ctr->check_period);
		int wsize = ctr->worker_queue->get_size(), csize = ctr->consumers.size();
		// std::cout << "Writer queue Size = " << ctr->writer_queue->get_size() << std::endl;
		if (wsize < ctr->low_threshold) {
			// scale down
			if (csize > 1) {
				std::cout << "Scaling down consumers from " << csize << " to " << csize - 1 << std::endl;
				Consumer* consumer = ctr->consumers.back();
				consumer->cancel();
				consumer->join();
				ctr->consumers.pop_back();
				delete consumer;
			}
		} else if (wsize > ctr->high_threshold) {
			// scale up
			std::cout << "Scaling up consumers from " << csize << " to " << csize + 1 << std::endl;
			Consumer* consumer = new Consumer(ctr->worker_queue, ctr->writer_queue, ctr->transformer);
			ctr->consumers.push_back(consumer);
			ctr->consumers.back()->start();
		}
	}
	return (void*) nullptr;
}

#endif // CONSUMER_CONTROLLER_HPP
