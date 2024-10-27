#ifndef EP_ONESHOT
#define EP_ONESHOT

class Game_Oneshot {
public:
  static Game_Oneshot& Instance();

  void SwitchSet(int switch_id, bool value);
private:
  enum Switches {
    EVENT_SETNAME = -1,
    HAS_ACCOUNT = -2,
  };

  enum Variables {
    PLAYERNAME = -1,
  };
};

inline Game_Oneshot& Oneshot() { return Game_Oneshot::Instance(); }

#endif
