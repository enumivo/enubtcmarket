#include "ex.hpp"

#include <cmath>
#include <enulib/action.hpp>
#include <enulib/asset.hpp>
#include "enu.token.hpp"

using namespace enumivo;
using namespace std;

void ex::receivedenu(const currency::transfer &transfer) {
  if (transfer.to != _self) {
    return;
  }

  // get ENU balance
  double enu_balance = enumivo::token(N(enu.token)).
	   get_balance(N(enu.btc.mm), enumivo::symbol_type(ENU_SYMBOL).name()).amount;
  
  enu_balance = enu_balance/10000;

  double received = transfer.quantity.amount;
  received = received/10000;

  // get BTC balance
  double btc_balance = enumivo::token(N(iou.coin)).
	   get_balance(N(enu.btc.mm), enumivo::symbol_type(BTC_SYMBOL).name()).amount;

  btc_balance = btc_balance/100000000;

  //deduct fee
  received = received * 0.99;
  
  double product = btc_balance * enu_balance;

  double buy = btc_balance - (product / (received + enu_balance));

  auto to = transfer.from;

  auto quantity = asset(100000000*buy, BTC_SYMBOL);

  action(permission_level{N(enu.btc.mm), N(active)}, N(iou.coin), N(transfer),
         std::make_tuple(N(enu.btc.mm), to, quantity,
                         std::string("Buy BTC with ENU")))
      .send();

  action(permission_level{_self, N(active)}, N(enu.token), N(transfer),
         std::make_tuple(_self, N(enu.btc.mm), transfer.quantity,
                         std::string("Buy BTC with ENU")))
      .send();
}

void ex::receivedbtc(const currency::transfer &transfer) {
  if (transfer.to != _self) {
    return;
  }

  // get BTC balance
  double btc_balance = enumivo::token(N(iou.coin)).
	   get_balance(N(enu.btc.mm), enumivo::symbol_type(BTC_SYMBOL).name()).amount;
  
  btc_balance = btc_balance/100000000;

  double received = transfer.quantity.amount;
  received = received/100000000;

  // get ENU balance
  double enu_balance = enumivo::token(N(enu.token)).
	   get_balance(N(enu.btc.mm), enumivo::symbol_type(ENU_SYMBOL).name()).amount;

  enu_balance = enu_balance/10000;

  //deduct fee
  received = received * 0.99;

  double product = enu_balance * btc_balance;

  double sell = enu_balance - (product / (received + btc_balance));

  auto to = transfer.from;

  auto quantity = asset(10000*sell, ENU_SYMBOL);

  action(permission_level{N(enu.btc.mm), N(active)}, N(enu.token), N(transfer),
         std::make_tuple(N(enu.btc.mm), to, quantity,
                         std::string("Sell BTC for ENU")))
      .send();

  action(permission_level{_self, N(active)}, N(iou.coin), N(transfer),
         std::make_tuple(_self, N(enu.btc.mm), transfer.quantity,
                         std::string("Sell BTC for ENU")))
      .send();
}

void ex::apply(account_name contract, action_name act) {

  if (contract == N(enu.token) && act == N(transfer)) {
    auto transfer = unpack_action_data<currency::transfer>();

    enumivo_assert(transfer.quantity.symbol == ENU_SYMBOL,
                 "Must send ENU");
    receivedenu(transfer);
    return;
  }

  if (contract == N(iou.coin) && act == N(transfer)) {
    auto transfer = unpack_action_data<currency::transfer>();

    enumivo_assert(transfer.quantity.symbol == BTC_SYMBOL,
                 "Must send BTC");
    receivedbtc(transfer);
    return;
  }

  if (contract == N(ftp.coin) && act == N(transfer)) {
    return;
  }

  if (act == N(transfer)) {
    auto transfer = unpack_action_data<currency::transfer>();
    enumivo_assert(false, "Must send BTC or ENU");
    return;
  }

  if (contract != _self) return;

}

extern "C" {
[[noreturn]] void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  ex enubtc(receiver);
  enubtc.apply(code, action);
  enumivo_exit(0);
}
}
