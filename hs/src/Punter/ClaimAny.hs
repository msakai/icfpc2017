{-# LANGUAGE DeriveGeneric #-}
{-# LANGUAGE DuplicateRecordFields #-}
{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE ScopedTypeVariables #-}
module Punter.ClaimAny where

import qualified Data.Aeson as J
import Data.Maybe (listToMaybe)
import Data.Set (Set, (\\))
import qualified Data.Set as Set
import GHC.Generics
import Protocol (PunterId)
import qualified Protocol as P
import Punter
import NormTypes (NRiver, toNRiver', deNRiver)

data Punter
  = Punter
  { setupInfo :: P.Setup
  , availableRivers :: Set NRiver
  , myRivers :: Set NRiver
  }
  deriving (Generic, Show)

instance J.ToJSON Punter
instance J.FromJSON Punter

takePunterId :: P.Setup -> PunterId
takePunterId = P.punter

instance Punter.IsPunter Punter where
  setup s =
    P.ReadyOn
    { P.ready   = takePunterId s
    , P.state   = Just $
        Punter
        { setupInfo = s
        , availableRivers = Set.fromList [ toNRiver' s' t' | P.River s' t' <- P.rivers (P.map s)]
        , myRivers = Set.empty
        }
    , P.futures = Nothing
    }

  applyMoves (P.Moves moves) p1@Punter{ setupInfo = si, availableRivers = availableRivers1, myRivers = myRivers1 } =
    p1
    { availableRivers = availableRivers1 \\ Set.fromList [ toNRiver' s t | P.MvClaim _punter' s t <- moves ]
    , myRivers = myRivers1 `Set.union` Set.fromList [ toNRiver' s t | P.MvClaim punter' s t <- moves, punter' == punterId ]
    }
    where
      punterId = takePunterId si

  chooseMoveSimple Punter{ setupInfo = si, availableRivers = availableRivers1 } =
    case listToMaybe $ Set.toList $ availableRivers1 of
      Nothing -> P.MvPass punterId
      Just r | (s,t) <- deNRiver r -> P.MvClaim punterId s t
    where
      punterId = takePunterId si
