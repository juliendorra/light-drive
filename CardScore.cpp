#include "CardScore.h"
namespace cardscore {
chsave::Store saves;
bool available() {
    const auto state=saves.status();
    return state==chsave::Status::Ready || state==chsave::Status::Empty;
}
}
