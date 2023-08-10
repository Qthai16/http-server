#! /usr/bin/python3

import sys
from enum import Enum
import random

# python decorator
# python generator (yield keyword)

# from collections import Counter, defaultdict
# some_list = [1,1,11,1,1,1,1,1,3,4,4,5,5,6,6,77,78]
# Counter(some_list)
# Counter("The quick fox jumps over the lazy dog")
# import string
# alphabetLetters = Counter(string.ascii_lowercase)

# import os
# import shutil
# os.walk, os.getcwd, shutil.move

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

class BColors:
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
        self.value = suit.value*rank.value

    def __str__(self) -> str:
        name = ""
        if (self.rank.value <= 10):
            name = self.rank.value # use number
        else:
            name = Card.high_rank_shorten[self.rank.name]
        if (self.suit.name in ["DIAMONDS", "HEARTS"]):
            return f"{BColors.RED}{name}{Card.pretty_suit[self.suit.name]}{BColors.ENDC}"
        else:
            return f"{name}{Card.pretty_suit[self.suit.name]}"
        

    def __lt__(self, other: "Card") -> bool:
        return self.value < other.value
        # return self.rank.value < other.rank.value or self.suit.value < other.suit.value

    def __eq__(self, other: "Card") -> bool:
        return self.value == other.value

class Deck():
    def __init__(self, cards: list[Card] = list()) -> None:
        self.cards = cards
        if (len(self.cards) == 0):
            for suit in list(Suit):
                for rank in list(Rank):
                    self.cards.append(Card(suit=suit, rank=rank))

    def __str__(self) -> str:
        return f"[{' '.join(str(card) for card in self.cards)}]"
        # outputStr = ""
        # for card in self.cards:
        #     outputStr += card.__str__()
        # return outputStr

    def __len__(self):
        return len(self.cards)

    def __iter__(self):
        return DeckIterator(self)
    
    def draw_top_cards(self, number = 1):
        drawCards = []
        for _ in range(number):
            drawCards.append(self.cards.pop(0))
        return Deck(drawCards)
    
    def add_bottom_cards(self, cards: list[Card]):
        self.cards.extend(cards)

class DeckIterator:
    def __init__(self, deck: Deck):
        self.deck = deck
        self.index = 0

    def __next__(self):
        if (self.index >= len(self.deck)):
            raise StopIteration
        result = self.deck.cards[self.index]
        self.index += 1
        return result

    
class Player():
    def __init__(self, name: str) -> None:
        self.name = name
        self.cards = None
        # self.cards = cards # set of cards

    def assign_cards(self, cards: Deck):
        self.cards = cards

    def draw_top_cards(self, number = 1):
        return self.cards.draw_top_cards(number)

    def add_bottom_cards(self, added_cards):
        self.cards.add_bottom_cards(added_cards)
        return Deck(added_cards)

class Game():
    def __init__(self, players: dict[Player]) -> None:
        # init Deck, shuffle, split to 2 deck of cards and assign to each player
        # init player and assign deck to each player
        self.deck = Deck()
        # print(f"before sort: {self.deck}")
        # self.deck.cards.sort()
        # print(f"after sort: {self.deck}")
        random.shuffle(self.deck.cards)
        print(f"shuffle deck: {self.deck}")
        number_of_players = len(players)
        self.players = players
        for player in self.players:
            pass

    def game_on(self) -> bool:
        for player in self.players:
            if len(player.cards) == 0:
                return False
        return True

    def run(self):
        while self.game_on():
            # each player draw top card
            # add card at random index or at the bottom
            # if card1 > card2:
                # add two cards to player 1
            # elif card1 < card2:
                # add two cards to player 2
            # else:
                # while at_war:
                # each one draws top 3 cards
            pass
        pass



if __name__ == "__main__":
    # print(f"after shuffle: {newDeck}")
    # sammy = Player(name="Sammy")
    # sarah = Player(name="Sarah")
    # new_war_game = Game({"Sammy": sammy, "Sarah": sarah})

    # test first
    deck = Deck()
    random.shuffle(deck.cards)
    sammy = Player(name="Sammy")
    sammy.assign_cards(deck)
    print(f"{sammy.name} cards: {sammy.cards}") # use decorator to printing after draw/add card
    print(f"{sammy.name} draws top card: {sammy.draw_top_cards()}")
    print(f"{sammy.name} draws top 3 card: {sammy.draw_top_cards(3)}")
    print(f"{sammy.name} draws add 1 card to bottom: {sammy.add_bottom_cards([Card(Suit.CLUBS, Rank.TWO)])}")
    print(f"{sammy.name} cards: {sammy.cards}")
    print(f"{sammy.name} draws add 2 card to bottom: {sammy.add_bottom_cards([Card(Suit.SPADES, Rank.SEVEN), Card(Suit.HEARTS, Rank.KING)])}")
    print(f"{sammy.name} cards: {sammy.cards}")
    # test end

    # print(f"sammy cards: {sammy.cards}")
    # print(f"draw 3 cards: {sammy.draw_card(3)}")
    # print(f"remain cards: {sammy.cards}")
    # print(f"draw 1 card: {sammy.draw_card()}")
    # print(f"remain cards: {sammy.cards}")
    # print(f"draw a card: {new_draw_cards}")
    # print(f"remain cards: {sammy.cards}")
    # print(f"type of card: {type(Card(Suit.HEARTS, Rank.TWO))}")
    # print(f"draw 3 card: {sammy.draw_card(3)}")
    # print(f"remain cards: {sammy.cards}")
    # print(f"heap 1: {newDeck[:len(newDeck)/2]}")
    # print(f"heap 2: {newDeck[len(newDeck)/2+1:]}")
    # new_card = Card(Suit.CLUBS, Rank.TWO)
    # print("{}, {}".format(new_card.rank.value, new_card.suit.value))
    check_compare_cards = [
        (Card(Suit.CLUBS, Rank.TWO), Card(Suit.CLUBS, Rank.KING)),
        (Card(Suit.CLUBS, Rank.TWO), Card(Suit.CLUBS, Rank.TWO)),
        (Card(Suit.DIAMONDS, Rank.TEN), Card(Suit.HEARTS, Rank.TEN))
    ]
    for first, second in check_compare_cards:
        print(f"{first} lt {second}: {first < second}")
        print(f"{first} eq {second}: {first == second}")
        print(f"{first} gt {second}: {first > second}")
