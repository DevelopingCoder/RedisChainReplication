import unittest
from unittest.mock import patch, MagicMock
from client import Client

class TestClientMethods(unittest.TestCase):

    @patch('client.Client.connect_to_master')
    def setUp(self, mock_method):
        self.client = Client()

    def tearDown(self):
        self.client.master_socket.close()

    @patch('client.socket')
    def test_master_connection(self, socket_mod):
        """
        Test that the client calls connect to the Master Server
        """
        socket_mock = MagicMock()
        socket_mod.socket.return_value = socket_mock
        fresh_client = Client()
        socket_mock.connect.assert_called_once_with(self.client.master_addr)

    def test_get_servers_with_loaded_cache(self):
        def raise_exception():
            raise Exception('send_request should not be called')
        with patch.dict(self.client.objID_servers, {'k1':'server1', 'k2':'server2'}):
            self.client.send_request = MagicMock(side_effect=raise_exception)
            self.assertEqual(self.client.get_servers('k1'), 'server1')
            self.assertEqual(self.client.get_servers('k2'), 'server2')

    def test_get_servers_general(self):
        """
        Requests obj k1, which should send a request to master
        Requests obj k1 again, which should load from cache
        """
        self.client.send_request = MagicMock()
        self.client.wait_for_reply = MagicMock(return_value='{"HEAD": ["head_addr", "head_port"], "TAIL": ["tail_addr", "tail_port"]}')

        self.assertEqual(self.client.get_servers('k1'), (('head_addr','head_port'), ('tail_addr','tail_port')))
        # Calling again to receive from cache
        self.assertEqual(self.client.get_servers('k1'), (('head_addr','head_port'), ('tail_addr','tail_port')))
        #assert send_request was only called once
        self.client.send_request.assert_called_once_with('k1', client_socket=self.client.master_socket)

    def test_wait_for_reply(self):
        # Mock a server that sends in bursts
        client_socket = MagicMock()
        orig_msg = 'hello there!'
        msg = orig_msg + '\n'

        def send_replies(arg):
            nonlocal msg
            msg_to_send = msg[:3]
            msg = msg[3:]
            return msg_to_send

        client_socket.recv = MagicMock(side_effect=send_replies)
        self.assertEqual(self.client.wait_for_reply(client_socket), orig_msg)

if __name__ == '__main__':
    unittest.main()