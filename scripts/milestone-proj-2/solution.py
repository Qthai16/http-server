#! /usr/bin/python3

import sys
from enum import Enum
import random

class Suit(Enum):
    SPADES = 1
    CLUBS = 2
    DIAMONDS = 3
    HEARTS = 4

class Rank(Enum):
    TWO=2
    THREE=3
    FOUR=4
    FIVE=5
    SIX=6
    SEVEN=7
    EIGHT=8
    NINE=9
    TEN=10
    JACK=11
    QUEEN=12
    KING=13
    ACE=14

class bcolors:
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    MAGENTA = '\033[95m'
    CYAN = '\033[96m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

class Card:
    # rank_order = [rank.value for rank in Rank]
    pretty_suit= {
        "SPADES": "♠",
        "CLUBS": "♣",
        "DIAMONDS": "♦",
        "HEARTS": "♥"
    }
    high_rank_shorten = {
        "JACK": "J",
        "QUEEN": "Q",
        "KING": "K",
        "ACE": "A"
    }
    def __init__(self, suit: Suit, rank: Rank) -> None:
        self.suit = suit
        self.rank = rank

    def __str__(self) -> str:
        name = ""
        if (self.rank.value <= 10):
            name = self.rank.value # use number
        else: name = Card.high_rank_shorten[self.rank.name]
        if (self.suit.name in ["DIAMONDS", "HEARTS"]):
            return f"{bcolors.RED}{name}{Card.pretty_suit[self.suit.name]}{bcolors.ENDC} "
        else: return f"{name}{Card.pretty_suit[self.suit.name]} "
        

    def __lt__(self, other: "Card") -> bool:
        return self.rank.value < other.rank.value or self.suit.value < other.suit.value

    # def __eq__(self, other: "Card") -> bool:
    #     return self.rank.value == other.rank.value

class Deck():
    def __init__(self, cards: list[Card] = list()) -> None:
        self.cards = cards
        for suit in list(Suit):
            for rank in list(Rank):
                self.cards.append(Card(suit=suit, rank=rank))

    def __str__(self) -> str:
        outputStr = ""
        for card in self.cards:
            outputStr += card.__str__()
        return outputStr

    def __len__(self):
        return len(self.cards)
    
class Player():
    def __init__(self, name: str, cards: set[Card]) -> None:
        self.name = name
        self.cards = cards # set of cards

    def draw_card(self, number = 1):
        drawCards = []
        for _ in range(0, number):
            drawCards.append(self.cards.pop())
        return drawCards

    def add_card(self, added_cards):
        self.cards.add(added_cards)

    def remove_card(self, remove_cards):
        self.cards.remove(remove_cards)

    def current_cards(self) -> int :
        return len(self.cards)



class Game():
    def __init__(self, number_of_players = 2) -> None:
        # init Deck, shuffle to 2 deck of cards and assign to each player
        # init player and assign deck to each player
        self.player1 = Player("Kevin", set())
        self.player2 = Player("Sammy", set())
        pass

    def game_can_still_run(self) -> bool:
        return self.player1.current_cards() > 0 or self.player2.current_cards() > 0

    def run(self):
        while self.game_can_still_run():
            # each player draw 1 card
            # if card1 > card2:
                # add two card to player 1
            # elif card1 < card2:
                # add two card to player 2
            # else:
                # each one draws 3 card
            pass
        pass



if __name__ == "__main__":
    newDeck = Deck()
    print(f"before sort: {newDeck}")
    newDeck.cards.sort()
    print(f"after sort: {newDeck}")
    print("shuffling deck")
    random.shuffle(newDeck.cards)
    print(f"after shuffle: {newDeck}")
    print("len: {}".format(len(newDeck)))
    # new_card = Card(Suit.CLUBS, Rank.TWO)
    # print("{}, {}".format(new_card.rank.value, new_card.suit.value))
    print(Card(Suit.CLUBS, Rank.TWO) < Card(Suit.CLUBS, Rank.KING)) # expect True
    print(Card(Suit.CLUBS, Rank.TWO) == Card(Suit.CLUBS, Rank.TWO)) # expect True
    print(Card(Suit.DIAMONDS, Rank.TEN) > Card(Suit.HEARTS, Rank.TEN)) # expect False
