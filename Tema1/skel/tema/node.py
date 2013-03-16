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

        
        
        #with self.print_lock:
        #    if DEBUG:
        #        print "%s data_store_max_req: %d"%(self.node_ID, max_pending_req)
        """n = self.block_size
        block1 = []
        for i in xrange(n):
            row = []
            for j in xrange(n):
                row.append(self.data_store.get_element_from_a(self.node_ID, i, j))
            block1.append(row)
        with dbg_lock:#self.print_lock:
            print "Node %s blockA is"%(str(self.node_ID))
            self.__mprint(block1)"""

    def __mprint(self, matrix):
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
            thread.join


