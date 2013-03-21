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
        self.nodes_mat = None       # Nodes saved in a matrix
        self.node_lock = Lock()

        # Number of max pending requests
        self.max_pending_req = self.data_store.get_max_pending_requests(self.node_ID)
        # Queue for DataStore connections
        self.data_store_queue = Queue(self.max_pending_req)

        # Thread for connections to Data Store
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

        # Get all necessary pieces from both matrices to make the product
        mat_a = [[0 for i in xrange(self.matrix_size)] for j in xrange(self.matrix_size)]
        mat_b = [[0 for i in xrange(self.matrix_size)] for j in xrange(self.matrix_size)]
        mat = {self.MAT_A: mat_a, self.MAT_B: mat_b}
        with print_lock:
            if DEBUG:
                self.mprint(mat[self.MAT_A])
        receive_queue = Queue()

        # Number of requests to be sent
        num_requests = self.matrix_size * (num_rows + num_columns)

        # Create all requests and temporarily store them
        for k in xrange(self.matrix_size):
            for i in xrange(start_row, start_row + num_rows):
                # Create a request for elements from A
                node = self.nodes_mat[i / self.block_size][k / self.block_size]
                request = Request(receive_queue, self.MAT_A, (i % self.block_size,
                    k % self.block_size), self.block_size, (i, k))

                node.data_store_queue.put(request)
            for j in xrange(start_column, start_column + num_columns):
                # Create a request for elements from B
                node = self.nodes_mat[k / self.block_size][j / self.block_size]
                request = Request(receive_queue, self.MAT_B, (k % self.block_size,
                    j % self.block_size), self.block_size, (k, j))
    
                node.data_store_queue.put(request)

        # Wait for requests responses and store them for this task accordingly
        while (num_requests > 0):
            response = receive_queue.get()
            if DEBUG:
                with print_lock:
                    print "mat id", response.matrix_identifier
            i = response.matrix_identifier[0]
            j = response.matrix_identifier[1]
            mat[response.selector][i][j] = response.result
            num_requests -= 1

        # Make the actual matrix multiplication for the requested block
        for i in xrange(start_row, start_row + num_rows):
            for j in xrange(start_column, start_column + num_columns):
                for k in xrange(self.matrix_size):
                    a, b = (mat_a[i][k], mat_b[k][j])
                    matrix_block[i - start_row][j - start_column] += a * b
        
        return matrix_block

    def mprint(self, matrix):
        """
            Prints a matrix hold as a row-major list of lists

            @param matrix: The matrix to be printed
        """
        n = len(matrix)
        for i in xrange(n):
            for j in xrange(len(matrix[i])):
                print matrix[i][j]," ",
            print

        print "--------------------------"

    def shutdown(self):
        """
            Instructs the node to shutdown (terminate all threads).
        """
        # Inform the wait_thread that it must exit
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
            
            # When no new request is about to come, exit from the thread
            if request == None:
                self.data_store_queue.put(None)
                return
            self.__process_request(request)
            self.data_store_queue.task_done()

    def __process_request(self, request):
        """
            Processes a request
            
            @type request: a Request
            @param request: The request to be processed
        """
        queue = request.queue
        result = 0
        if request.selector == self.MAT_A:
            with print_lock:
                if DEBUG:
                    print request.node_identifier
            result = self.data_store.get_element_from_a(self.node_ID,
                request.node_identifier[0], request.node_identifier[1])
        elif request.selector == self.MAT_B:
            result = self.data_store.get_element_from_b(self.node_ID,
                request.node_identifier[0], request.node_identifier[1])

        # Put the response in the requester's receive_queue
        response = Response(request.selector, request.matrix_identifier, result)
        queue.put(response)

class Request:
    """
        This class defines a request that will be sent from a Node to another
        Node for obtaining elements from Data Store
    """
    def __init__(self, queue, selector, node_identifier, block_size, matrix_identifier):
        """
            Constructor
            @type queue: A blocking queue of type Queue.Queue
            @param queue: The queue where to put the response for this request
            @type selector: Boolean
            @param selector: Selects the matrix from which to get the element
            @type node_identifier: Tuple of two integers (i, j)
            @param node_identifier: Identifies the node in the matrix of nodes
            @type block_size: Integer
            @param block_size: The block_size of a Data Store
            @type matrix_identifier: Tuple of two integers (i, j)
            @param matrix_identifier: Identifies the position in the matrix of elements
        """
        self.queue = queue
        self.selector = selector
        self.node_identifier = node_identifier
        self.block_size = block_size
        self.matrix_identifier = matrix_identifier

class Response:
    """
        This class defines a response for a request
    """
    def __init__(self, selector, matrix_identifier, result):
        """
            Constructor
            @type selector: Boolean
            @param selector: Selects the matrix from which to get the element
            @type matrix_identifier: Tuple of two integers (i, j)
            @param matrix_identifier: Identifies the position in the matrix of elements
            @param result: The element that was requested
        """
        self.selector = selector
        self.matrix_identifier = matrix_identifier
        self.result = result
