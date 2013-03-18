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

DEBUG = True
print_lock = Lock()

class Node:
    """
        Class that represents a cluster node with computation and storage functionalities.
    """

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
        matrix_block = [[0 for i in range(num_columns)] for j in range(num_rows)]
        t = Thread(target = self.__main_work, args = (start_row,
                                start_column, num_rows, num_columns, matrix_block))
        t.start()

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
        self.data_store.register_thread(self.node_ID)

        for i in xrange(start_row, start_row + num_rows):
            for j in xrange(start_column, start_column + num_columns):
                for k in xrange(self.matrix_size):
                    a, b = self.obtain_elements(i, j, k)
                    matrix_block[i - start_row][j - start_row] += a * b
        
        #with self.print_lock:
        #    if DEBUG:
        #        print "%s data_store_max_req: %d"%(self.node_ID, max_pending_req)
        """n = self.block_size
        block1 = []
        for i in xrange(n):
            row = []
            for j in xrange(n):
                row.append(self.data_store.get_element_from_a(self, i, j))
            block1.append(row)
        with dbg_lock:#self.print_lock:
            print "Node %s blockA is"%(str(self.node_ID))
            self.mprint(block1)"""

    def obtain_elements(self, i, j, k):
        """
            Gets elements A[i][k] and B[k][j] from matrices to be multiplied
            
            @param i: index for line of matrix A
            @param j: index for column of matrix B
            @param k: index for line/column of both matrices
            
            @return: a tuple (a, b) where a contains A[i][k] and b contains
                B[k][j]
        """
        if (i >= self.node_ID[0] * self.block_size and
            i < (self.node_ID[0] + 1) * self.block_size and
            j >= self.node_ID[1] * self.block_size and
            j < (self.node_ID[1] + 1) * self.block_size):
            pass
        self.data_store_queue.put((i,j), True)
        with print_lock:
            if DEBUG:
                print self.node_ID, " entered ", (i, j), " in queue"
        return (0, 0)

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
            thread.join()
        self.data_store_queue.put(None)
        self.wait_thread.join()
        with print_lock:
            if DEBUG:
                print self.node_ID, " joined all threads"

    def __wait_request(self):
        """
            Waits for requests from another Nodes
        """
        while True:
            request = self.data_store_queue.get(True)
            if request == None:
                self.data_store_queue.put(None, True)
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
        pass
