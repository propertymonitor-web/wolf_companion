// included at the end of a scene .cpp: in a mystery build's per-species file this hands the scene out as a PetSceneBase
#ifdef PET_NS
struct PetSceneImpl : public PetSceneBase {
  WolfScene s;
  explicit PetSceneImpl(WolfGame* g) : s(g) {}
  int render(DisplayDriver& d, uint32_t now, uint16_t batt) override { return s.render(d, now, batt); }
};
PET_NS_CLOSE
#define PET_MK2(ns) make_##ns
#define PET_MK(ns) PET_MK2(ns)
PetSceneBase* PET_MK(PET_NS)(WolfGame* g) { return new PET_NS::PetSceneImpl(g); }
#else
PET_NS_CLOSE
#endif
