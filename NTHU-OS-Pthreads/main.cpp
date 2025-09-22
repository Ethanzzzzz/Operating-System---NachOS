#include <assert.h>
#include <stdlib.h>
#include "ts_queue.hpp"
#include "item.hpp"
#include "reader.hpp"
#include "writer.hpp"
#include "producer.hpp"
#include "consumer_controller.hpp"

#define READER_QUEUE_SIZE 200
#define WORKER_QUEUE_SIZE 200
#define WRITER_QUEUE_SIZE 4000
#define CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGE 20
#define CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGE 80
#define CONSUMER_CONTROLLER_CHECK_PERIOD 1000000
#define PRODUCER_NUM 4

int main(int argc, char** argv) {
	assert(argc == 4);

	int n = atoi(argv[1]);
	std::string input_file_name(argv[2]);
	std::string output_file_name(argv[3]);

	// TODO: implements main function
	TSQueue<Item*> *reader_queue = new TSQueue<Item*>(READER_QUEUE_SIZE);
	TSQueue<Item*> *worker_queue = new TSQueue<Item*>(WORKER_QUEUE_SIZE);
	TSQueue<Item*> *writer_queue = new TSQueue<Item*>(WRITER_QUEUE_SIZE);
	Transformer *transformer = new Transformer();

	Producer **producers = new Producer *[PRODUCER_NUM];
	Reader *reader = new Reader(n, input_file_name, reader_queue);
	Writer *writer = new Writer(n, output_file_name, writer_queue);
	ConsumerController *consumer_controller = new ConsumerController(worker_queue, writer_queue, transformer, CONSUMER_CONTROLLER_CHECK_PERIOD, WORKER_QUEUE_SIZE * CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGE / 100, WORKER_QUEUE_SIZE * CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGE / 100);

	// std::cout << "Start" << std::endl;
	// std::cout << "Reader started" << std::endl;
	reader->start();
	// std::cout << "Writer started" << std::endl;
	writer->start();
	// std::cout << "Consumer Controller started" << std::endl;
	consumer_controller->start();
	for(int i=0; i<PRODUCER_NUM; i++){
		producers[i] = new Producer(reader_queue, worker_queue, transformer);
		// std::cout << "Producer " << i << " started" << std::endl;
		producers[i]->start();
	}

	// wait for all threads to finish
	// std::cout << "Waiting for reader to terminate" << std::endl;
	reader->join();
	// std::cout << "Waiting for writer to terminate" << std::endl;
	writer->join();

	delete reader;
	delete writer;

	return 0;
}
