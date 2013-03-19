"""
    This module represents a cluster's computational node.

    Computer Systems Architecture course
    Assignment 1 - Cluster Activity Simulation
    march 2013

    Constantin Serban-Radoi 333CA
"""

from random import random
from threading import *
from time import sleep
from Queue import Queue

#DEBUG = True
DEBUG = False
print_lock = Lock()

class Node:
    """
        Class that represents a cluster node with computation and storage functionalities.
    """
    MAT_A = True
    MAT_B = False

    def __init__(self, node_ID, block_size, matrix_size, data_store):
        """
            Constructor.

            @param node_ID: a pair of IDs uniquely identifying the node;
            IDs are integers between 0 and matrix_size/block_size
            @param block_size: the size of the matrix blocks stored in this node's datastore
            @param matrix_size: the size of the matrix
            @param data_store: reference to the node's local data store
        """
        self.node_ID = node_ID
        self.block_size = block_size
        self.matrix_size = matrix_size
        self.data_store = data_store

        self.nodes = []             # Nodes connected to
        self.nodes_mat = None
        self.node_lock = Lock()
        self.node_threads = set()   # Threads for this Node
        self.print_lock = Lock()

        # Number of max pending requests
        self.max_pending_req = self.data_store.get_max_pending_requests(self.node_ID)
        # Queue for DataStore connections
        self.data_store_queue = Queue(self.max_pending_req)

        self.condition = Condition()    # Condition for colaboration with threads
        self.not_finished_all = True

        self.wait_thread = Thread(target = self.__wait_request)
        self.wait_thread.start()

    def set_nodes(self, nodes):
        """
            Informs the current node of the other nodes in the cluster.
            Guaranteed to be called before the first call to compute_matrix_block.

            @param nodes: a list containing all the nodes in the cluster
        """
        with self.node_lock:
            self.nodes = nodes
            n = self.matrix_size / self.block_size
            self.nodes_mat = [[0 for i in xrange(n)] for j in xrange(n)]
            for node in self.nodes:
                self.nodes_mat[node.node_ID[0]][node.node_ID[1]] = node

    def compute_matrix_block(self, start_row, start_column, num_rows, num_columns):
        """
            Computes a given block of the result matrix.
            The method invoked by FEP nodes.

            @param start_row: the index of the first row in the block
            @param start_column: the index of the first column in the block
            @param num_rows: number of rows in the block
            @param num_columns: number of columns in the block

            @return: the block of the result matrix encoded as a row-order list of lists of integers
        """
        matrix_block = [[0 for i in xrange(num_columns)] for j in xrange(num_rows)]
        t = Thread(target = self.__main_work, args = (start_row,
                                start_column, num_rows, num_columns, matrix_block))
        t.start()

        t.join()
        self.node_threads.add(t)

        return matrix_block

    def __main_work(self, start_row, start_column, num_rows, num_columns, matrix_block):
        """
            Computes a given block of the result matrix.
            Makes the necessary communication between Nodes

            @param start_row: the index of the first row in the block
            @param start_column: the index of the first column in the block
            @param num_rows: number of rows in the block
            @param num_columns: number of columns in the block

            @param matrix_block: the block of the result matrix encoded as a row-order list of lists of integers
        """
        #self.data_store.register_thread(self.node_ID)

        mat_a = [[0 for i in xrange(self.matrix_size)] for j in xrange(self.matrix_size)]
        mat_b = [[0 for i in xrange(self.matrix_size)] for j in xrange(self.matrix_size)]
        mat = {self.MAT_A: mat_a, self.MAT_B: mat_b}
        with print_lock:
            if DEBUG:
                self.mprint(mat[self.MAT_A])
        receive_queue = Queue()

        # Number of requests to be sent
        num_requests = self.matrix_size * (num_rows + num_columns)

        for k in xrange(self.matrix_size):
            for i in xrange(start_row, start_row + num_rows):
                # Create a request for elements from A
                node = self.nodes_mat[i / self.block_size][k / self.block_size]
                
                node.data_store_queue.put([receive_queue, self.MAT_A, 
                    i % self.block_size, k % self.block_size, self.block_size,
                    i, k])
            for j in xrange(start_column, start_column + num_columns):
                # Create a request for elements from B
                node = self.nodes_mat[k / self.block_size][j / self.block_size]
                node.data_store_queue.put([receive_queue, self.MAT_B,
                    k % self.block_size, j % self.block_size, self.block_size,
                    k, j])

        while (num_requests > 0):
            response = receive_queue.get()
            if DEBUG:
                with print_lock:
                    print '[', response[5], ',', response[6], ']'
            mat[response[0]][response[5]][response[6]] = response[3]
            num_requests -= 1
            
        """with print_lock:
            print 'MAT_A'
            self.mprint(mat_a)
            print 'MAT_B'
            self.mprint(mat_b)"""

        for i in xrange(start_row, start_row + num_rows):
            for j in xrange(start_column, start_column + num_columns):
                for k in xrange(self.matrix_size):
                    a, b = (mat_a[i][k], mat_b[k][j])
                    matrix_block[i - start_row][j - start_column] += a * b
        
        """with print_lock:
            print 'matrix_block'
            self.mprint(matrix_block)"""
        

    def mprint(self, matrix):
        n = len(matrix)
        for i in range(n):
            for j in range(len(matrix[i])):
                print matrix[i][j]," ",
            print

        print "--------------------------"

    def shutdown(self):
        """
            Instructs the node to shutdown (terminate all threads).
        """
        for thread in self.node_threads:
            #thread.join()
            del thread
        self.data_store_queue.put(None)
        self.wait_thread.join()
        del self.wait_thread
        with print_lock:
            if DEBUG:
                print self.node_ID, " joined all threads"

    def __wait_request(self):
        """
            Waits for requests from another Nodes
        """
        self.data_store.register_thread(self.node_ID)
        while True:
            request = self.data_store_queue.get()
            if request == None:
                self.data_store_queue.put(None)
                return
            self.__process_request(request)
            self.data_store_queue.task_done()

    def __process_request(self, request):
        """
            Processes a request
            
            @type request: tuple representing (node_ID, reference to response which is
                a list)
            @param request: The request to be processed
        """
        # Request contains: request[0] - Queue where to place the response
        #   request[1] - Bool for matrix selection
        #   request[2] - Lines index in the block
        #   request[3] - Columns index in the block
        #   request[4] - Block size
        #   request[5] - Lines index in the full matrix
        #   request[6] - Columns index in the full matrix
        queue = request[0]
        result = 0
        if request[1] == self.MAT_A:
            with print_lock:
                if DEBUG:
                    print request[2], request[3]
            result = self.data_store.get_element_from_a(self.node_ID, request[2], request[3])
        elif request[1] == self.MAT_B:
            result = self.data_store.get_element_from_b(self.node_ID, request[2], request[3])

        queue.put([request[1], request[2], request[3], result, request[4], request[5], request[6]])
