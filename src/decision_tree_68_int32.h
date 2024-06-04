// !!! This file is generated using emlearn !!!
#include <eml_trees.h>
    

static const EmlTreesNode dt_nodes[31] = {
  { 538, 1486294784.000000f, 1, 16 },
  { 2888, 1234973248.000000f, 1, 8 },
  { 551, 1230283520.000000f, 1, 4 },
  { 527, 1329215744.000000f, 1, 2 },
  { 559, 1349546176.000000f, -1, -2 },
  { 2842, 1268559808.000000f, -2, -1 },
  { 2653, 1350167616.000000f, 1, 2 },
  { 2794, 1382819072.000000f, -2, -1 },
  { 530, 1475137920.000000f, -1, -2 },
  { 554, 1532208448.000000f, 1, 4 },
  { 3827, 1638499264.000000f, 1, 2 },
  { 519, 1602504832.000000f, -1, -1 },
  { 280, 1559095808.000000f, -2, -1 },
  { 2617, 1531292864.000000f, 1, 2 },
  { 2898, 1435583424.000000f, -2, -2 },
  { 2073, 1549487552.000000f, -2, -1 },
  { 2609, 1620326208.000000f, 1, 8 },
  { 2844, 1564411072.000000f, 1, 4 },
  { 541, 1407047296.000000f, 1, 2 },
  { 2092, 1529315712.000000f, -2, -1 },
  { 2468, 1713982592.000000f, -2, -2 },
  { 549, 1631000704.000000f, 1, 2 },
  { 2083, 1536218752.000000f, -2, -1 },
  { 2099, 1907181376.000000f, -2, -1 },
  { 2074, 1651743104.000000f, 1, 4 },
  { 401, 1626120576.000000f, 1, 2 },
  { 2450, 1504646464.000000f, -2, -1 },
  { 2026, 1733553920.000000f, -2, -2 },
  { 393, 1770605056.000000f, 1, 2 },
  { 2818, 1481313856.000000f, -2, -1 },
  { 2695, 1692056768.000000f, -2, -1 } 
};

static const int32_t dt_tree_roots[1] = { 0 };

static const uint8_t dt_leaves[2] = { 0, 1 };

EmlTrees dt = {
        31,
        (EmlTreesNode *)(dt_nodes),	  
        1,
        (int32_t *)(dt_tree_roots),
        2,
        (uint32_t *)(dt_leaves),
        0,
        6063,
        2,
    };

static inline int32_t dt_tree_0(const float *features, int32_t features_length) {
          if (features[538] < 1486294784.000000f) {
              if (features[2888] < 1234973248.000000f) {
                  if (features[551] < 1230283520.000000f) {
                      if (features[527] < 1329215744.000000f) {
                          if (features[559] < 1349546176.000000f) {
                              return 0;
                          } else {
                              return 1;
                          }
                      } else {
                          if (features[2842] < 1268559808.000000f) {
                              return 1;
                          } else {
                              return 0;
                          }
                      }
                  } else {
                      if (features[2653] < 1350167616.000000f) {
                          if (features[2794] < 1382819072.000000f) {
                              return 1;
                          } else {
                              return 0;
                          }
                      } else {
                          if (features[530] < 1475137920.000000f) {
                              return 0;
                          } else {
                              return 1;
                          }
                      }
                  }
              } else {
                  if (features[554] < 1532208448.000000f) {
                      if (features[3827] < 1638499264.000000f) {
                          if (features[519] < 1602504832.000000f) {
                              return 0;
                          } else {
                              return 0;
                          }
                      } else {
                          if (features[280] < 1559095808.000000f) {
                              return 1;
                          } else {
                              return 0;
                          }
                      }
                  } else {
                      if (features[2617] < 1531292864.000000f) {
                          if (features[2898] < 1435583424.000000f) {
                              return 1;
                          } else {
                              return 1;
                          }
                      } else {
                          if (features[2073] < 1549487552.000000f) {
                              return 1;
                          } else {
                              return 0;
                          }
                      }
                  }
              }
          } else {
              if (features[2609] < 1620326208.000000f) {
                  if (features[2844] < 1564411072.000000f) {
                      if (features[541] < 1407047296.000000f) {
                          if (features[2092] < 1529315712.000000f) {
                              return 1;
                          } else {
                              return 0;
                          }
                      } else {
                          if (features[2468] < 1713982592.000000f) {
                              return 1;
                          } else {
                              return 1;
                          }
                      }
                  } else {
                      if (features[549] < 1631000704.000000f) {
                          if (features[2083] < 1536218752.000000f) {
                              return 1;
                          } else {
                              return 0;
                          }
                      } else {
                          if (features[2099] < 1907181376.000000f) {
                              return 1;
                          } else {
                              return 0;
                          }
                      }
                  }
              } else {
                  if (features[2074] < 1651743104.000000f) {
                      if (features[401] < 1626120576.000000f) {
                          if (features[2450] < 1504646464.000000f) {
                              return 1;
                          } else {
                              return 0;
                          }
                      } else {
                          if (features[2026] < 1733553920.000000f) {
                              return 1;
                          } else {
                              return 1;
                          }
                      }
                  } else {
                      if (features[393] < 1770605056.000000f) {
                          if (features[2818] < 1481313856.000000f) {
                              return 1;
                          } else {
                              return 0;
                          }
                      } else {
                          if (features[2695] < 1692056768.000000f) {
                              return 1;
                          } else {
                              return 0;
                          }
                      }
                  }
              }
          }
        }
        

int32_t dt_predict(const float *features, int32_t features_length) {

        int32_t votes[2] = {0,};
        int32_t _class = -1;

        _class = dt_tree_0(features, features_length); votes[_class] += 1;
    
        int32_t most_voted_class = -1;
        int32_t most_voted_votes = 0;
        for (int32_t i=0; i<2; i++) {

            if (votes[i] > most_voted_votes) {
                most_voted_class = i;
                most_voted_votes = votes[i];
            }
        }
        return most_voted_class;
    }
    