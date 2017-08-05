{-# OPTIONS_GHC -Wall #-}
module Main where

import Data.Proxy
import OnlinePlay
import qualified Punter.Pass as PassPunter
import qualified Punter.ClaimAny as AnyPunter
import qualified Punter.ClaimGreedy as GreedyPunter
import System.IO
import System.Environment

main :: IO ()
main = do
  hSetBuffering stdout LineBuffering
  [name, port] <- getArgs
  case name of
    "pass" -> runPunterOnline (Proxy :: Proxy PassPunter.Punter) port
    "any" -> runPunterOnline (Proxy :: Proxy AnyPunter.Punter) port
    "greedy" -> runPunterOnline (Proxy :: Proxy GreedyPunter.Punter) port
    _ -> error "unknown punter algorithm"
