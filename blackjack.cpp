#include <iostream>
#include <array>
#include <stack>
#include <unordered_map>
#include <chrono>
#include <map>
#include <cfloat>
#include <vector>
#include <boost/python.hpp>

using namespace std;
namespace p = boost::python;

enum Card {
    ANY, ACE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE, TEN
};

const array<Card, 10> allCards{ACE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE, TEN};

class Shoe {

public:

    Shoe(const Shoe& shoe){
        nCard = shoe.nCard;
    }

    Shoe(const size_t decks = 0){
        nCard[0] = decks*52;
        nCard[1] = nCard[2] = nCard[3] = nCard[4] = nCard[5]
        = nCard[6] = nCard[7] = nCard[8] = nCard[9] = decks*4;
        nCard[10] = decks*16;
    }

    double sample(const Card& card) const {
        return (1.0*nCard[card])/(nCard[0]);
    }

    void remove(const Card& card){
        nCard[0]--;
        nCard[card]--;
    }

    void add(const Card& card){
        nCard[0]++;
        nCard[card]++;
    }

private:
    array<size_t, 11> nCard;
};

enum Move {
    HIT, SPLIT, STAND, DOUBLE, RACE
};

class SubHand {

public:

    SubHand(const SubHand& subHand){
        cards = subHand.cards;
        nCards = subHand.nCards;
        total = subHand.total;
        soft = subHand.soft;
    }

    SubHand(const Card& first = ANY, const Card& second = ANY){
        nCards = 2;
        if (second == ANY){
            cards[0] = second;
            cards[1] = first;
        } else {
            cards[0] = first;
            cards[1] = second;
        }
        total = cards[0] + cards[1];
        if (cards[0] == ACE || cards[1] == ACE){
            soft = true;
        }
        wager = 1;
    }

    bool operator==(const SubHand& subHand) const {
        return subHand.wager==wager&&((subHand.total==total&&subHand.nCards==2&&nCards==2)
            ||(isBust()&&subHand.isBust()));
    }

    size_t getWager() const{
        return wager;
    }

    size_t getTotal() const{
        if (soft && total<=11){
            return total+10;
        }else{
            return total;
        };
    }

    void expose(const Card& card){
        cards[0] = card;
        total += card;
    }

    void backExpose(){
        total -= cards[0];
        cards[0] = ANY;
    }

    void hit(const Card& card){
        cards[nCards] = card;
        nCards++;
        total+=card;
    }

    void backHit(){
        nCards--;
        total-=cards[nCards];
    }

    void doubleDown(const Card& card){
        hit(card);
        wager*=2;
    }

    void backDoubleDown(){
        backHit();
        wager/=2;
    }

    array<SubHand, 2> split(const Card& first, const Card& second) const {
        array<SubHand, 2> hands;
        hands[0] = SubHand(cards[0], first);
        hands[1] = SubHand(cards[1], second);
        return hands;
    };

    void backSplit(const SubHand& first, const SubHand& second){
        cards[0] = first.cards[0];
        cards[1] = second.cards[0];
        total = cards[0]+cards[1];
        soft = cards[0] == ACE;
    }

    bool isFresh() const {
        return nCards == 2;
    }

    bool isExposed() const {
        return cards[0] != ANY;
    }

    bool isDoubles() const {
        return cards[0] == cards[1];
    }

    bool isSoft() const {
        return soft&&total<=11;
    }

    bool isBust() const {
        return total>21;
    }

    bool blackjack() const {
        return nCards==2 && getTotal() == 21;
    }

    int profit(const SubHand& dealer) const {
        size_t playerTotal = getTotal();
        size_t dealerTotal = dealer.getTotal();
        bool playerBj = blackjack();
        bool dealerBj = dealer.blackjack();
        if (playerBj && dealerBj){
            return 0;
        }
        if (playerBj){
            return (wager*3)/2;
        }
        if (dealerBj){
            return -wager;
        }
        if (playerTotal>21){
            return -wager;
        }
        if (dealerTotal>21){
            return wager;
        }
        if (playerTotal > dealerTotal){
            return wager;
        } else if (playerTotal==dealerTotal){
            return 0;
        } else {
            return -wager;
        }
    }

    friend std::ostream& operator<<(std::ostream& os, const SubHand& subHand) {
        os<<"(";
        for (size_t i=0;i<subHand.nCards - 1;i++){
            os<<subHand.cards[i]<<", ";
        }
        os<<subHand.cards[subHand.nCards - 1]<<")";
        return os;
    }

private:
    size_t nCards;
    array<Card, 20> cards;
    size_t total;
    int wager;
    bool soft = false;
};

enum HandState {
    COMPLETE, DOUBLES, FRESH, OPEN
};

std::ostream& operator<<(std::ostream& os, const HandState& handState){
    switch(handState){
        case COMPLETE:
            return os<<"Complete";
        case DOUBLES:
            return os<<"Doubles";
        case FRESH:
            return os<<"Fresh";
        case OPEN:
            return os<<"Open";
        default:
            return os<<"Unknown hand state";
    }
}

class Hand {

public:

    Hand(const Hand& hand){
        active = hand.active;
        nSubHands = hand.nSubHands;
        subHands = hand.subHands;
    }

    Hand(const Card& first = ANY, const Card& second = ANY){
        subHands[0] = SubHand(first, second);
        if (subHands[0].getTotal()==21){
            active = 2;
        }else {
            active = 0;
        }
        nSubHands = 1;
    }

    HandState getState(){
        if (complete()){
            return COMPLETE;
        }
        if (subHands[active].isFresh()){
            if (nSubHands == 1 && subHands[0].isDoubles()){
                return DOUBLES;
            }else{
                return FRESH;
            }
        }
        return OPEN;
    }

    size_t nHands(){
        return nSubHands;
    }

    size_t getActive(){
        return active;
    }

    SubHand& getSubHand(size_t index) {
        return subHands[index];
    }

    void stand(){
        advanceSubHand();
    }

    void backStand(){
        backAdvanceSubHand();
    }

    void hit(const Card& card){
        subHands[active].hit(card);
        if (subHands[active].getTotal()>=21){
            advanceSubHand();
        }
    }

    void backHit(){
        if (active==2 || subHands[active].isFresh()){
            backAdvanceSubHand();
        }
        subHands[active].backHit();
    }

    void doubleDown(const Card& card){
        subHands[active].doubleDown(card);
        advanceSubHand();
    }

    void backDoubleDown(){
        backAdvanceSubHand();
        subHands[active].backDoubleDown();
    }

    void split(const Card& first, const Card& second){
        subHands = subHands[active].split(first, second);
        nSubHands++;
        if (subHands[0].getTotal()>=21){
            if (subHands[1].getTotal()>=21){
                active=2;
            }else{
                active=1;
            }
        }
    }

    void backSplit(){
        subHands[0].backSplit(subHands[0], subHands[1]);
        nSubHands = 1;
        active = 0;
    }

    bool complete(){
        return active==2;
    }

    bool isSoft(){
        return subHands[active].isSoft();
    }

    int profit(const SubHand& dealer){
        int profit = 0;
        for (size_t i=0;i<nSubHands;i++){
            profit += subHands[i].profit(dealer);
        }
        return profit;
    }

    friend std::ostream& operator<<(std::ostream& os, const Hand& hand){
        os<<"(";
        for (size_t i=0;i<hand.nSubHands;i++){
            if (i != 0) {
                os << ", ";
            }
            if (hand.active == i) {
                os << "A: ";
            }
            os<<hand.subHands[i];
        }
        os<<")";
        return os;
    }

private:

    size_t nSubHands;
    size_t active;

    array<SubHand, 2> subHands;

    void advanceSubHand() {
        if (active == 0) {
            if (nSubHands == 2) {
                active = (subHands[1].getTotal() >= 21)?2:1;
            } else {
                active = 2;
            }
        } else{
            active++;
        }
    }

    void backAdvanceSubHand(){
        if (active==2){
            if (nSubHands==2 && !subHands[1].isFresh()){
                active = 1;
            }else{
                active=0;
            }
        } else if (active==1){
            active=0;
        }
    }

};

std::ostream& operator<<(std::ostream& os, const Move& move) {
    switch(move){
        case HIT:
            return os<<"Hit";
        case SPLIT:
            return os<<"Split";
        case STAND:
            return os<<"Stand";
        case DOUBLE:
            return os<<"Double";
        case RACE:
            return os<<"Race";
        default:
            return os<<"Unknown move";
    }
}

struct Result {

    Result(const double ev, const Card& nextCard, const Move& move) : ev(ev), nextCard(nextCard), move(move) {};

    Result(const double ev, const Card& nextCard) : ev(ev), nextCard(nextCard), move(STAND) {};

    Result(const double ev, const Move& move) : ev(ev), nextCard(ANY), move(move) {};


    double ev;
    Card nextCard;
    Move move;
};

std::ostream& operator<<(std::ostream& os, const Result& result) {
    return os << "Next card: " << result.nextCard << ", Move: " << result.move << ", EV: " << result.ev;
}

class Game {

public:

    Game(const Shoe& shoe, const Hand& hand, const SubHand& dealer){
        this->shoe = shoe;
        this->hand = hand;
        this->dealer = dealer;
    }

    void stand(){
        hand.stand();
    }

    void split(const Card& first, const Card& second){
        shoe.remove(first);
        shoe.remove(second);
        hand.split(first, second);
    }

    void doubleDown(const Card& card){
        shoe.remove(card);
        hand.doubleDown(card);
    }

    void hit(const Card& card){
        shoe.remove(card);
        hand.hit(card);
    }

    double profit(const SubHand& player, const Card& nextCard=ANY){
        size_t v0 = player.getWager();
        size_t v1 = nextCard;//0-10
        size_t v2 = dealer.getTotal();//1-11
        size_t v3 = 0;
        if (player.isBust()){
            v3 = 22;
        } else if (player.blackjack()){
            v3 = 23;
        } else{
            v3 = player.getTotal();
        }
        size_t idx = v0 + v1*2 + v2*2*11 + v3*2*11*12;
        if (profitLookup[idx]==DBL_MAX){
            profitLookup[idx] = pushDealer(player, nextCard);
        }
        return profitLookup[idx];
    }

    double pushDealer(const SubHand& player, const Card& nextCard=ANY){
        double ev = 0.0;
        if (player.isBust()){
            return player.profit(dealer);
        }
        if (!dealer.isExposed()){
            for (Card card1: allCards){
                double p1 = shoe.sample(card1);
                shoe.remove(card1);
                dealer.expose(card1);
                ev += p1 * pushDealer(player, nextCard);
                dealer.backExpose();
                shoe.add(card1);
            }
        } else if (nextCard!=ANY){
            shoe.remove(nextCard);
            if (dealer.getTotal()<17 || (dealer.isSoft()&&dealer.getTotal()==17)) {
                dealer.hit(nextCard);
                ev += pushDealer(player);
                dealer.backHit();
            } else {
                ev += player.profit(dealer);
            }
            shoe.add(nextCard);
        } else {
            for (Card card1: allCards) {
                double p1 = shoe.sample(card1);
                shoe.remove(card1);
                if (dealer.getTotal() < 17 || (dealer.isSoft() && dealer.getTotal() == 17)) {
                    dealer.hit(card1);
                    ev += p1 * pushDealer(player);
                    dealer.backHit();
                } else {
                    ev += p1 * player.profit(dealer);
                }
                shoe.add(card1);
            }
        }
        return ev;
    }

    Result pushSubHand(SubHand& player, const Card& nextCard, const bool isFinal){
        double doubleEV = 0.0;
        double hitEV = 0.0;
        double standEV = profit(player, isFinal?(Card)nextCard:ANY);
        if (player.getTotal()>=21){
            return Result(standEV, nextCard);
        }
        double p0 = 1.0;
        size_t nCards = nextCard==ANY?allCards.size():1;
        const Card* cards = nextCard==ANY?&allCards[0]:&nextCard;
        for (size_t i=0;i<nCards;i++){
            const Card card1 = cards[i];
            double p1 = nCards==1?1.0:shoe.sample(card1);
            //shoe.remove(card1);
            if (player.isFresh()){
                player.doubleDown(card1);
                doubleEV += p1*profit(player, ANY);
                player.backDoubleDown();
            }
            player.hit(card1);
            if (player.isBust()){
                hitEV += p0*profit(player, ANY);
                player.backHit();
                //shoe.add(card1);
                break;
            }
            hitEV += p1*pushSubHand(player, ANY, isFinal).ev;
            player.backHit();
            //shoe.add(card1);
            p0 -= p1;
        }
        if (doubleEV>standEV && doubleEV>hitEV && player.isFresh()){
            return Result(doubleEV, ANY);
        } else if (standEV>hitEV){
            return Result(standEV, nextCard);
        } else {
            return Result(hitEV, ANY);
        }
    }

    Result calculateNextMove(const Card& nextCard){
        double hitEV = 0.0;
        double doubleEV = 0.0;
        double standEV = 0.0;
        double splitEV = 0.0;
        HandState state = hand.getState();
        if (state == COMPLETE){
            return Result(-1E100, nextCard, Move::STAND);
        } else {
            size_t nCards = nextCard==ANY?allCards.size():1;
            const Card* cards = nextCard==ANY?&allCards[0]:&nextCard;
            for (size_t i=0;i<nCards;i++) {
                const Card card1 = cards[i];
                double p1 = nCards==1?1.0:shoe.sample(card1);
                //shoe.remove(card1);
                if (state == DOUBLES){
                    for (const Card card2: allCards){
                        double p2 = shoe.sample(card2);
                        //shoe.remove(card2);
                        hand.split(card1, card2);
                        splitEV+=p1*p2*pushSubHand(hand.getSubHand(0), ANY, false).ev;
                        splitEV+=p1*p2*pushSubHand(hand.getSubHand(1), ANY, false).ev;
                        hand.backSplit();

                        //shoe.add(card2);
                    }
                }


                if (state == DOUBLES || state == FRESH) {
                    hand.doubleDown(card1);
                    doubleEV+=p1*pushSubHand(hand.getSubHand(0), ANY, false).ev;
                    hand.backDoubleDown();
                }
                if (state == OPEN || state == DOUBLES || state == FRESH){
                    hand.hit(card1);
                    hitEV+=p1*pushSubHand(hand.getSubHand(0), ANY, false).ev;
                    hand.backHit();
                }
                //shoe.add(card1);
            }
            if (state == OPEN || state == DOUBLES || state == FRESH){
                hand.stand();
                standEV+=profit(hand.getSubHand(0), nextCard);
                hand.backStand();
            }

            cout<<"standEv: "<<standEV<<", hitEV: "<<hitEV<<", doubleEV: "<<doubleEV<<", splitEV: "<<splitEV<<endl;
            if (state == DOUBLES && splitEV>doubleEV && splitEV>hitEV&&splitEV>standEV){
                return Result(splitEV, nextCard, SPLIT);
            } else if ((state == DOUBLES || state == FRESH) && doubleEV>hitEV && doubleEV>standEV){
                return Result(doubleEV, nextCard, DOUBLE);
            } else if ((state == OPEN || state == DOUBLES || state == FRESH) && hitEV>standEV){
                return Result(hitEV, nextCard, HIT);
            } else{
                return Result(standEV, nextCard, STAND);
            }
        }
    }


private:

    Shoe shoe;
    SubHand dealer;
    Hand hand;

    vector<double> profitLookup = vector<double>(24*12*11*2, DBL_MAX);

};

/*int main(){
    Shoe shoe(8);
    SubHand dealer(ANY, FOUR);
    Hand hand(FIVE, SIX);
    Game game(shoe, hand, dealer);
    cout<<game.calculateNextMove(TWO)<<endl;
    return 0;
}*/

BOOST_PYTHON_MODULE(libBlackjack){
        p::enum_<Card>("Card")
                .value("ACE", Card::ACE)
                .value("TWO", Card::TWO)
                .value("THREE", Card::THREE)
                .value("FOUR", Card::FOUR)
                .value("FIVE", Card::FIVE)
                .value("SIX", Card::SIX)
                .value("SEVEN", Card::SEVEN)
                .value("EIGHT", Card::EIGHT)
                .value("NINE", Card::NINE)
                .value("TEN", Card::TEN)
                .value("ANY", Card::ANY);
        p::enum_<Move>("Move")
                .value("STAND", Move::STAND)
                .value("DOUBLE", Move::DOUBLE)
                .value("HIT", Move::HIT)
                .value("RACE", Move::RACE)
                .value("SPLIT", Move::SPLIT);
        p::class_<Shoe>("Shoe", p::init<const size_t>())
                .def("sample", &Shoe::sample)
                .def("remove", &Shoe::remove)
                .def("add", &Shoe::add);
        p::class_<SubHand>("SubHand", p::init<const Card, const Card>());
        p::class_<Hand>("Hand", p::init<const Card, const Card>());
        p::class_<Game>("Game", p::init<const Shoe&, const Hand&, const SubHand&>())
                .def("stand", &Game::stand)
                .def("hit", &Game::hit)
                .def("split", &Game::split)
                .def("double", &Game::doubleDown)
                .def("calculateNextMove", &Game::calculateNextMove);
        p::class_<Result>("Result", p::init<const double, const Card>())
                .def_readonly("ev", &Result::ev)
                .def_readonly("move", &Result::move)
                .def_readonly("nextCard", &Result::nextCard);


}
