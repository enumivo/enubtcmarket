/**
 *  @file
 *  @copyright defined in enumivo/LICENSE
 */

#include "enu.token.hpp"

namespace enumivo {

void token::create( account_name issuer,
                    asset        maximum_supply )
{
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    enumivo_assert( sym.is_valid(), "invalid symbol name" );
    enumivo_assert( maximum_supply.is_valid(), "invalid supply");
    enumivo_assert( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( _self, sym.name() );
    auto existing = statstable.find( sym.name() );
    enumivo_assert( existing == statstable.end(), "token with symbol already exists" );

    statstable.emplace( _self, [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
    });
}


void token::issue( account_name to, asset quantity, string memo )
{
    auto sym = quantity.symbol;
    enumivo_assert( sym.is_valid(), "invalid symbol name" );
    enumivo_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    auto sym_name = sym.name();
    stats statstable( _self, sym_name );
    auto existing = statstable.find( sym_name );
    enumivo_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;

    require_auth( st.issuer );
    enumivo_assert( quantity.is_valid(), "invalid quantity" );
    enumivo_assert( quantity.amount > 0, "must issue positive quantity" );

    enumivo_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    enumivo_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, 0, [&]( auto& s ) {
       s.supply += quantity;
    });

    add_balance( st.issuer, quantity, st.issuer );

    if( to != st.issuer ) {
       SEND_INLINE_ACTION( *this, transfer, {st.issuer,N(active)}, {st.issuer, to, quantity, memo} );
    }
}

void token::retire( asset quantity, string memo )
{
    auto sym = quantity.symbol;
    enumivo_assert( sym.is_valid(), "invalid symbol name" );
    enumivo_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    auto sym_name = sym.name();
    stats statstable( _self, sym_name );
    auto existing = statstable.find( sym_name );
    enumivo_assert( existing != statstable.end(), "token with symbol does not exist" );
    const auto& st = *existing;

    require_auth( st.issuer );
    enumivo_assert( quantity.is_valid(), "invalid quantity" );
    enumivo_assert( quantity.amount > 0, "must retire positive quantity" );

    enumivo_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

    statstable.modify( st, 0, [&]( auto& s ) {
       s.supply -= quantity;
    });

    sub_balance( st.issuer, quantity );
}

void token::transfer( account_name from,
                      account_name to,
                      asset        quantity,
                      string       memo )
{
    enumivo_assert( from != to, "cannot transfer to self" );
    require_auth( from );
    enumivo_assert( is_account( to ), "to account does not exist");

    /////////////////////////////////////////////////////////////////////////////
    //don't allow enumivo.prods to recieve tokens
    enumivo_assert( to != N(enumivo.prods), "enumivo.prods prohibited to receive");

    auto sym = quantity.symbol.name();
    stats statstable( _self, sym );
    const auto& st = statstable.get( sym );

    require_recipient( from );
    require_recipient( to );

    enumivo_assert( quantity.is_valid(), "invalid quantity" );
    enumivo_assert( quantity.amount > 0, "must transfer positive quantity" );
    enumivo_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    enumivo_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    auto payer = has_auth( to ) ? to : from;

    sub_balance( from, quantity );
    add_balance( to, quantity, payer );
}

void token::sub_balance( account_name owner, asset value ) {
   accounts from_acnts( _self, owner );

   const auto& from = from_acnts.get( value.symbol.name(), "no balance object found" );
   enumivo_assert( from.balance.amount >= value.amount, "overdrawn balance" );

   from_acnts.modify( from, owner, [&]( auto& a ) {
         a.balance -= value;
      });
}

void token::add_balance( account_name owner, asset value, account_name ram_payer )
{
   accounts to_acnts( _self, owner );
   auto to = to_acnts.find( value.symbol.name() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, 0, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

void token::open( account_name owner, symbol_type symbol, account_name ram_payer )
{
   require_auth( ram_payer );
   accounts acnts( _self, owner );
   auto it = acnts.find( symbol.name() );
   if( it == acnts.end() ) {
      acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = asset{0, symbol};
      });
   }
}

void token::close( account_name owner, symbol_type symbol )
{
   require_auth( owner );
   accounts acnts( _self, owner );
   auto it = acnts.find( symbol.name() );
   enumivo_assert( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
   enumivo_assert( it->balance.amount == 0, "Cannot close because the balance is not zero." );
   acnts.erase( it );
}

} /// namespace enumivo

ENUMIVO_ABI( enumivo::token, (create)(issue)(transfer)(open)(close)(retire) )
