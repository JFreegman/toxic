=============
API Reference
=============

Messages
========
.. function:: display(msg)

   Display a message to the user through the current window.

   :param msg: The message to display.
   :type msg: string
   :rtype: none

.. function:: send(msg)

   Send a message to the user specified by the currently open conversation.

   :param msg: The message to display.
   :type msg: string
   :rtype: none


State
=====
.. function:: get_nick()

   Return the user's current nickname.

   :rtype: string

.. function:: get_status()

   Return a string representing the user's current status. Can be either "online", "away", or "busy".

   :rtype: string

.. function:: get_status_message()

   Return the user's current status message.

   :rtype: string

.. function:: get_all_friends()

   Return a list of all the user's friends.

   :rtype: list of (string, string) tuples containing the nickname followed by their public key


Commands
========
.. function:: execute(command, class)

   Execute the given command, where a class of 0 indicates a global command, 1 indicates a chat command, and 2 indicates a groupchat command.

   :param command: The command to execute.
   :type command: string
   :param class: The class of the command.
   :type class: int
   :rtype: none

.. function:: register(command, help, callback)

   Register a callback to be executed whenever command is run. The callback function will be called with one argument, a list of arguments from when the user calls the command.

   :param command: The command to listen for.
   :type command: string
   :param help: A description of the command to be shown in the help menu.
   :type help: string
   :param callback: The function to be called.
   :type callback: callable
   :rtype: none
