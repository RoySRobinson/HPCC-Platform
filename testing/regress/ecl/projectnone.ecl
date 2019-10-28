/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems®.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
############################################################################## */

import $.setup;
prefix := setup.Files(false, false).QueryFilePrefix;

#onwarning (1021, ignore); // Ignore warning that all output values are constant

r1 := RECORD
   string35 code;
  END;

r2 := RECORD
  integer2 id;
  embedded DATASET(r1) child;
 END;

mk1(unsigned c) := TRANSFORM(r1, SELF.code := (string)c; SELF := []);
mk2(unsigned c) := TRANSFORM(r2, SELF.id := c; SELF.child := DATASET((c-1) % 5, mk1(COUNTER+c)), SELF := []);

d := DATASET(20, mk2(counter));
sequential(
    output(d,,prefix+'myspill', overwrite);
    output(DATASET(prefix+'myspill', r2, THOR), { one := 1 });
    output(count(nocombine(DATASET(prefix+'myspill', r2, THOR))));
);
