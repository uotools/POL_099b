/*
History
=======


Notes
=======

*/

#ifndef PARTY_H
#define PARTY_H

#include "pktdef.h"
#include "syshook.h"
#include "proplist.h"
#include <vector>
namespace Pol {
  namespace Clib {
    class StreamWriter;
  }
  namespace Mobile {
	class Character;
  }
  namespace Network {
	class Client;
  }
  namespace Module {
    class EPartyRefObjImp;
  }

  namespace Core {
	class NPC;
    struct PKTBI_BF;
    struct PKTBI_6C;

	void handle_add( Network::Client* client, PKTBI_BF* msg );
	void handle_remove( Network::Client* client, PKTBI_BF* msg );
	void handle_member_msg( Network::Client* client, PKTBI_BF* msg );
	void handle_party_msg( Network::Client* client, PKTBI_BF* msg );
	void handle_loot_perm( Network::Client* client, PKTBI_BF* msg );
	void handle_accept_invite( Network::Client* client, PKTBI_BF* msg );
	void handle_decline_invite( Network::Client* client, PKTBI_BF* msg );

	class Party : public ref_counted
	{
	public:
	  explicit Party( Clib::ConfigElem& elem );
	  explicit Party( u32 serial );
	  bool is_leader( u32 serial ) const;
      bool is_member( u32 serial ) const;
      bool is_candidate( u32 serial ) const;
	  bool register_with_members();
	  bool add_candidate( u32 serial );
	  bool add_member( u32 serial );
	  void add_offline_mem( u32 serial );
	  bool remove_candidate( u32 serial );
	  bool remove_member( u32 serial );
	  bool remove_offline_mem( u32 serial );
	  void set_leader( u32 serial );
	  void send_remove_member( Mobile::Character* chr, bool* disband );
	  void send_member_list( Mobile::Character* to_chr );
	  void send_msg_to_all( unsigned int clilocnr, const char* affix = 0, Mobile::Character* exeptchr = NULL ) const;
	  bool test_size() const;
	  bool can_add() const;
	  void disband();
	  void send_stat_to( Mobile::Character* chr, Mobile::Character* bob ) const;
	  void send_stats_on_add( Mobile::Character* newmember ) const;
	  void on_mana_changed( Mobile::Character* chr ) const;
	  void on_stam_changed( Mobile::Character* chr ) const;
	  void send_member_msg_public( Mobile::Character* chr, u16* wtext, size_t wtextlen ) const;
	  void send_member_msg_private( Mobile::Character* chr, Mobile::Character* tochr, u16* wtext, size_t wtextlen ) const;
	  u32 leader() const;
	  void printOn( Clib::StreamWriter& sw ) const;
	  u32 get_member_at( unsigned short pos ) const;
	  void cleanup();

	  friend class Module::EPartyRefObjImp;

	private:
	  std::vector<u32> _member_serials;
      std::vector<u32> _candidates_serials;
      std::vector<u32> _offlinemember_serials;
	  u32 _leaderserial;
	  PropertyList _proplist;
	};

	void send_invite( Mobile::Character* member, Mobile::Character* leader );
	void add_candidate( Mobile::Character* member, Mobile::Character* leader );
	void disband_party( u32 leader );
	void send_empty_party( Mobile::Character* chr );
	void on_loggon_party( Mobile::Character* chr );
	void on_loggoff_party( Mobile::Character* chr );
	void send_attributes_normalized( Mobile::Character* chr, Mobile::Character* bob );
	void invite_timeout( Mobile::Character* mem );
  }
}
#endif
