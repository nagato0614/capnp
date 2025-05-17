@0xbf5147bcd4c586e0;

$import "capnp/c++.capnp".namespace("onchange");

interface IOnChange {
  onChanged @0 (message: Text);
}

interface ChangeService {
  subscribe @0 (listener: IOnChange) -> ();
  triggerChange @1 () -> ();
}
