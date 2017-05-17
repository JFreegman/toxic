import toxic_api
import random

FORTUNES = [
    "A bug in the code is worth two in the documentation.",
    "A bug in the hand is better than one as yet undetected.",
    "\"A debugged program is one for which you have not yet found the "
    "conditions that make it fail.\" -- Jerry Ogdin"
]

def send_fortune(args):
    """Callback function that sends the contact of the current window a
    given number of random fortunes.
    """
    if len(args) != 1:
        toxic_api.display("Only one argument allowed!")
        return

    try:
        count = int(args[0])
    except ValueError:
        toxic_api.display("Argument must be a number!")

    name = toxic_api.get_nick()

    toxic_api.send("%s has decided to send you %d fortunes:" % (name, count))
    for _ in range(count):
        toxic_api.send(random.choice(FORTUNES))


toxic_api.register("/fortune", "Send a fortune to the contact of the current "
                   "window", send_fortune)
