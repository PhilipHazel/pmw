<?xml version="1.0" encoding="UTF-8"?>

<score-partwise>

  <work>
    <work-title>MusicXML Title</work-title>
  </work>

  <defaults>
    <scaling>
      <millimeters>4</millimeters>
      <tenths>40</tenths>
    </scaling>
    <page-layout>
      <page-height>1800</page-height>
      <page-width>1400</page-width>
      <page-margins type="both">
        <left-margin>117</left-margin>
        <right-margin>86</right-margin>
        <top-margin>99</top-margin>
        <bottom-margin>199</bottom-margin>
      </page-margins>
    </page-layout>
    <system-layout>
      <system-margins>
        <left-margin>0</left-margin>
        <right-margin>0</right-margin>
      </system-margins>
      <system-distance>80</system-distance>
      <top-system-distance>80</top-system-distance>
      <system-dividers>
        <left-divider print-object="yes"/>
        <right-divider print-object="no"/>
      </system-dividers>
    </system-layout>
    <staff-layout>
      <staff-distance>93</staff-distance>
    </staff-layout>
  </defaults>

  <credit page="1">
    <credit-type>rights</credit-type>
    <credit-words default-x="1412" default-y="45" font-size="7.8" halign="center" valign="bottom">Testing, testing</credit-words>
  </credit>

  <defaults>
  <scaling>
    <millimeters>6</millimeters>
    <tenths>40</tenths>
  </scaling>
  </defaults>
   
  <part-list>
    <score-part id="P1">
    </score-part>
  </part-list>


  <!--============================================================-->
  <part id="P1">
    <measure number="1" width="371">

      <attributes>
        <divisions>48</divisions>
        <key>
          <fifths>-2</fifths>
          <mode>major</mode>
        </key>
        <time symbol="common">
          <beats>4</beats>
          <beat-type>4</beat-type>
        </time>
        <clef>
          <sign>G</sign>
          <line>2</line>
        </clef>
      </attributes>

      <note default-x="20">
        <pitch>
          <step>D</step>
          <octave>5</octave>
        </pitch>
        <duration>48</duration>
        <voice>1</voice>
        <type>quarter</type>
        <stem default-y="-45">down</stem>
        <lyric default-y="-82" number="1">
          <syllabic>begin</syllabic>
          <text>Fle</text>
        </lyric>
      </note>
      
      <note default-x="131">
        <pitch>
          <step>C</step>
          <alter>1</alter>
          <octave>5</octave>
        </pitch>
        <duration>36</duration>
        <voice>1</voice>
        <type>eighth</type>
        <dot/>
        <accidental>sharp</accidental>
        <stem default-y="-50">down</stem>
        <beam number="1">begin</beam>
        <lyric default-y="-82" number="1">
          <syllabic>end</syllabic>
          <text>hen!</text>
          <end-line/>
        </lyric>
      </note>
      <note default-x="231">
        <pitch>
          <step>A</step>
          <octave>4</octave>
        </pitch>
        <duration>12</duration>
        <voice>1</voice>
        <type>16th</type>
        <stem default-y="-60">down</stem>
        <beam number="1">end</beam>
        <beam number="2">backward hook</beam>
        <lyric default-y="-82" number="1">
          <syllabic>single</syllabic>
          <text>Aus</text>
        </lyric>
      </note>
      <note default-x="264">
        <pitch>
          <step>C</step>
          <octave>5</octave>
        </pitch>
        <duration>36</duration>
        <voice>1</voice>
        <type>eighth</type>
        <dot/>
        <accidental>natural</accidental>
        <stem default-y="-50">down</stem>
        <beam number="1">begin</beam>
        <lyric default-y="-82" justify="left" number="1">
          <syllabic>begin</syllabic>
          <text>die</text>
        </lyric>
      </note>
      <note default-x="358">
        <pitch>
          <step>B</step>
          <alter>-1</alter>
          <octave>4</octave>
        </pitch>
        <duration>12</duration>
        <voice>1</voice>
        <type>16th</type>
        <stem default-y="-55">down</stem>
        <beam number="1">end</beam>
        <beam number="2">backward hook</beam>
        <lyric default-y="-82" number="1">
          <syllabic>end</syllabic>
          <text>sem</text>
        </lyric>
      </note>
      <note default-x="385">
        <pitch>
          <step>A</step>
          <octave>4</octave>
        </pitch>
        <duration>8</duration>
        <voice>1</voice>
        <type>16th</type>
        <time-modification>
          <actual-notes>3</actual-notes>
          <normal-notes>2</normal-notes>
        </time-modification>
        <stem default-y="-60">down</stem>
        <beam number="1">begin</beam>
        <beam number="2">begin</beam>
        <notations>
          <tuplet bracket="no" number="1" placement="above" type="start"/>
          <slur number="1" placement="above" type="start"/>
        </notations>
        <lyric default-y="-82" justify="left" number="1">
          <syllabic>begin</syllabic>
          <text>Fel</text>
        </lyric>
      </note>
      <note default-x="412">
        <pitch>
          <step>C</step>
          <octave>5</octave>
        </pitch>
        <duration>8</duration>
        <voice>1</voice>
        <type>16th</type>
        <time-modification>
          <actual-notes>3</actual-notes>
          <normal-notes>2</normal-notes>
        </time-modification>
        <stem default-y="-60">down</stem>
        <beam number="1">continue</beam>
        <beam number="2">continue</beam>
        <notations>
          <slur number="1" type="stop"/>
        </notations>
      </note>
      <note default-x="436">
        <pitch>
          <step>D</step>
          <octave>5</octave>
        </pitch>
        <duration>8</duration>
        <voice>1</voice>
        <type>16th</type>
        <time-modification>
          <actual-notes>3</actual-notes>
          <normal-notes>2</normal-notes>
        </time-modification>
        <stem default-y="-60">down</stem>
        <beam number="1">continue</beam>
        <beam number="2">end</beam>
        <notations>
          <tuplet number="1" type="stop"/>
        </notations>
        <lyric default-y="-82" number="1">
          <syllabic>end</syllabic>
          <text>sen,</text>
        </lyric>
      </note>
      <note default-x="466">
        <pitch>
          <step>E</step>
          <alter>-1</alter>
          <octave>5</octave>
        </pitch>
        <duration>8</duration>
        <voice>1</voice>
        <type>16th</type>
        <time-modification>
          <actual-notes>3</actual-notes>
          <normal-notes>2</normal-notes>
        </time-modification>
        <stem default-y="-60">down</stem>
        <beam number="1">continue</beam>
        <beam number="2">begin</beam>
        <notations>
          <tuplet bracket="no" number="1" placement="above" type="start"/>
          <slur number="1" placement="above" type="start"/>
        </notations>
        <lyric default-y="-82" justify="left" number="1">
          <syllabic>single</syllabic>
          <text>starr</text>
        </lyric>
      </note>
      <note default-x="499">
        <pitch>
          <step>C</step>
          <octave>5</octave>
        </pitch>
        <duration>8</duration>
        <voice>1</voice>
        <type>16th</type>
        <time-modification>
          <actual-notes>3</actual-notes>
          <normal-notes>2</normal-notes>
        </time-modification>
        <stem default-y="-60">down</stem>
        <beam number="1">continue</beam>
        <beam number="2">continue</beam>
        <notations>
          <slur number="1" type="stop"/>
        </notations>
      </note>
      <note default-x="535">
        <pitch>
          <step>A</step>
          <octave>4</octave>
        </pitch>
        <duration>8</duration>
        <voice>1</voice>
        <type>16th</type>
        <time-modification>
          <actual-notes>3</actual-notes>
          <normal-notes>2</normal-notes>
        </time-modification>
        <stem default-y="-60">down</stem>
        <beam number="1">end</beam>
        <beam number="2">end</beam>
        <notations>
          <tuplet number="1" type="stop"/>
        </notations>
        <lyric default-y="-82" number="1">
          <syllabic>single</syllabic>
          <text>und</text>
        </lyric>
      </note>

    </measure>
    


  <!--============================================================-->
    <measure number="2">
      <attributes>
        <divisions>8</divisions>
      </attributes>

      <direction placement="below">
        <direction-type> 
          <wedge type="crescendo"/>
        </direction-type> 
      </direction>   

      <note>
        <pitch>
          <step>G</step>
          <octave>4</octave>
        </pitch>
        <duration>8</duration>
        <type>quarter</type>
      </note>

      <note>
        <pitch>
          <step>G</step>
          <octave>4</octave>
        </pitch>
        <duration>8</duration>
        <type>quarter</type>
      </note>

    </measure>


  <!--============================================================-->
    <measure number="3">
      <note>
        <pitch>
          <step>G</step>
          <octave>4</octave>
        </pitch>
        <duration>8</duration>
        <type>quarter</type>
      </note>

      <note>
        <pitch>
          <step>G</step>
          <octave>4</octave>
        </pitch>
        <duration>8</duration>
        <type>quarter</type>
      </note>

      <direction>
        <direction-type> 
          <wedge type="stop"/>
        </direction-type> 
      </direction>   

    </measure>


  <!--============================================================-->
    <measure number="4">
      <note>
        <pitch>
          <step>G</step>
          <octave>4</octave>
        </pitch>
        <duration>8</duration>
        <type>quarter</type>
      </note>

      <attributes>
        <clef>
          <sign>C</sign>
          <line>3</line>
        </clef>
      </attributes>

      <note>
        <pitch>
          <step>G</step>
          <octave>4</octave>
        </pitch>
        <duration>8</duration>
        <type>quarter</type>
      </note>

    </measure>
    
  <!--============================================================-->
    <measure number="5">

      <note default-x="106">
        <rest measure="yes"/>
        <duration>8</duration>
        <type>quarter</type>
      </note>

    </measure>

  <!--============================================================-->
    <measure number="6">

      <note default-x="106">
        <rest measure="yes"/>
        <duration>8</duration>
        <type>quarter</type>
      </note>

    </measure>

  </part>

</score-partwise>
