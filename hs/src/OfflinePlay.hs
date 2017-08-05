{-# OPTIONS_GHC -Wall #-}
{-# LANGUAGE DuplicateRecordFields #-}
{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE ScopedTypeVariables #-}

module OfflinePlay where

import qualified Data.ByteString.Lazy.Char8 as L8
import Data.Char (isDigit)
import qualified Data.Aeson as J
import Data.Monoid
import Data.Proxy
import qualified Data.Text as T
import System.IO

import qualified Protocol as P
import Punter
import qualified Punter.Pass as PassPunter

test :: IO ()
test = runPunterOffline (Proxy :: Proxy PassPunter.Punter)

runPunterOffline :: Punter.IsPunter a => Proxy a -> IO ()
runPunterOffline punter = do
      hSetBuffering stdin (BlockBuffering Nothing)
      hSetBuffering stdout (BlockBuffering Nothing)
      runPunterOffline' "sampou-offline" punter

runPunterOffline' :: forall a. Punter.IsPunter a => T.Text -> Proxy a -> IO ()
runPunterOffline' name _ =
    loop
  where
    loop = do
      send (P.HandshakePunter{ P.me=name })
      (_::P.HandshakeServer) <- decodeIO "handshake" =<< recv
      jsonv <- decodeIO "multiplex" =<< recv :: IO J.Value
      case J.fromJSON jsonv of
        J.Success (moves :: P.PrevMoves a) -> do  --- check gameplay first
          let move = Punter.play moves
          send move
          loop

        J.Error _  -> case J.fromJSON jsonv of
          J.Success (setupInfo :: P.Setup) -> do
            let ready = Punter.setup setupInfo :: P.Ready a
            send ready
            loop

          J.Error _ -> case J.fromJSON jsonv of
            J.Success (_ :: P.Scoring)  ->  return ()
            J.Error _  -> fail ("unknown messsage: " ++ show jsonv)

send :: J.ToJSON a => a -> IO ()
send x = do
  let json = J.encode x
  L8.hPutStrLn stderr ("-> " <> json)
  hFlush stderr
  L8.hPutStr stdout $ L8.pack (show (L8.length json)) <> ":" <> json
  hFlush stdout

recv :: IO L8.ByteString
recv = do
  len <- getLength []
  s <- L8.hGet stdin len
  L8.hPutStrLn stderr $ "<- " <> s
  return s
  where
    getLength cs = do
      c <- hGetChar stdin
      if isDigit c then getLength (c:cs) else return (read (reverse cs))

decodeIO :: J.FromJSON a => String -> L8.ByteString -> IO a
decodeIO name s =
  maybe
  (fail $ "failed to parse: " ++ name ++ ": " ++ show s)
  return
  $ J.decode s
