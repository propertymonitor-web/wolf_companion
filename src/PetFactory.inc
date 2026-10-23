// included at the end of a scene .cpp: in a mystery build's per-species file this hands the scene out as a PetSceneBase
#include <stddef.h>
#ifdef PET_NS
struct PetSceneImpl : public PetSceneBase {
  WolfScene s;
  explicit PetSceneImpl(WolfGame* g) : s(g) {}
  int render(DisplayDriver& d, uint32_t now, uint16_t batt) override { return s.render(d, now, batt); }
#if !PET_INSECT
  void setHead(uint8_t head_species) override { s.setHead(head_species); }
#endif
};
#if !PET_INSECT
// the pictures, seen through the look-alike structures of PetArtView.h (the layouts must match exactly)
static_assert(sizeof(WolfPart) == sizeof(PaPart) && sizeof(WolfKey) == sizeof(PaKey) && sizeof(WolfStageArt) == sizeof(PaStage), "art layouts differ");
static_assert(offsetof(WolfStageArt, head) == offsetof(PaStage, head) && offsetof(WolfStageArt, key) == offsetof(PaStage, key) &&
              offsetof(WolfStageArt, ear) == offsetof(PaStage, ear) && offsetof(WolfStageArt, tail) == offsetof(PaStage, tail) &&
              offsetof(WolfStageArt, legs) == offsetof(PaStage, legs), "art layouts differ");
#endif
PET_NS_CLOSE
#define PET_MK2(ns) make_##ns
#define PET_MK(ns) PET_MK2(ns)
#define PET_ARTFN2(ns) art_##ns
#define PET_ARTFN(ns) PET_ARTFN2(ns)
PetSceneBase* PET_MK(PET_NS)(WolfGame* g) { return new PET_NS::PetSceneImpl(g); }
#if !PET_INSECT
const PaStage* const* PET_ARTFN(PET_NS)() { return (const PaStage* const*)PET_NS::wolf_art_stages; }
#endif
#else
PET_NS_CLOSE
#endif
