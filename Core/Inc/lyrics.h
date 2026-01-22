#ifndef LYRICS_H
#define LYRICS_H

char lyrics_romaji[][40] = {
    // Verse
    "Nagareteku toki no naka de demo",
    "Kedarusa ga hora guruguru mawatte",
    "Watashi kara hanareru kokoro mo",
    "Mienaiwa sou shiranai?",
    "Jibun kara ugoku koto mo naku",
    "Toki no sukima ni nagasare tsuzukete",
    "Shiranai wa mawari no koto nado",
    "Watashi wa watashi sore dake",
    // Refrain
    "Yume miteru? Nani mo mitenai?",
    "Kataru mo muda na jibun no kotoba",
    "Kanashimu nante tsukareru dake yo",
    "Nani mo kanjizu sugoseba ii no",
    "Tomadou kotoba ataerarete mo",
    "Jibun no kokoro tada uwa no sora",
    "Moshi watashi kara ugoku no naraba",
    "Subete kaeru no nara kuro ni suru",
    // Chorus
    "Konna jibun ni mirai wa aru no?",
    "Konna sekai ni watashi wa iru no?",
    "Ima setsunai no? Ima kanashii no?",
    "Jibun no koto mo wakaranai mama",
    "Ayumu koto sae tsukareru dake yo",
    "Hito no koto nado shiri mo shinaiwa",
    "Konna watashi mo kawareru no nara",
    "Moshi kawareru no nara shiro ni naru",
    // Verse (repeat)
    // Refrain (repeat)
    // Outro
    "Ugoku no naraba ugoku no naraba",
    "Subete kowasuwa subete kowasuwa",
    "Kanashimu naraba kanashimu naraba",
    "Watashi no kokoro shiroku kawareru?",
    "Anata no koto mo watashi no koto mo",
    "Subete no koto mo mada shiranai no",
    "Omoi mabuta wo aketa no naraba",
    "Subete kowasu no nara kuro ni nare!"
};

const char lyrics_english[][61] = {
    // Verse
    "Ever on and on I continue circling",
    "With nothing but my hate in a carousel of agony",
    "Till slowly I forget and my heart starts vanishing",
    "And suddenly I see that I can't break free, I'm",
    "Slipping through the cracks of a dark eternity",
    "With nothing but my pain and the paralyzing agony",
    "To tell me who I am, who I was, uncertainty",
    "Enveloping my mind till I can't break free, and",
    // Refrain
    "Maybe it's a dream; maybe nothing else is real",
    "But it wouldn't mean a thing if I told you how I feel",
    "So I'm tired of all the pain, all the misery inside",
    "And I wish that I could live feeling nothing but the night",
    "You can tell me what to say; you can tell me where to go",
    "But I doubt that I would care, and my heart would never know",
    "If I make another move there'll be no more turning back",
    "Because everything will change and it all will fade to black",
    // Chorus
    "Will tomorrow ever come? Will I make it through the night?",
    "Will there ever be a place for the broken in the light?",
    "Am I hurting? Am I sad? Should I stay, or should I go?",
    "I've forgotten how to tell. Did I ever even know?",
    "Can I take another step? I've done everything I can",
    "All the people that I see I will never understand",
    "If I find a way to change, if I step into the light",
    "Then I'll never be the same and it all will fade to white",
    // Verse (repeat)
    // Refrain (repeat)
    // Outro
    "If I make another move, if I take another step",
    "Then it all would fall apart. There'd be nothing of me left",
    "If I'm crying in the wind, if I'm crying in the night",
    "Will there ever be a way? Will my heart return to white?",
    "Can you tell me who you are? Can you tell me where I am?",
    "I've forgotten how too see; I've forgotten if I can",
    "If I opened up my eyes there'd be no more going back",
    "'Cause I'd throw it all away and it all would fade to black"
};

typedef struct {
    uint16_t start_frame;
    uint16_t end_frame;
    uint8_t lyric_index;
} LyricEvent;

const LyricEvent lyric_events[] = {
    // Verse
    {  436,  486,  0 },
    {  486,  540,  1 },
    {  540,  592,  2 },
    {  592,  643,  3 },
    {  643,  696,  4 },
    {  696,  749,  5 },
    {  749,  799,  6 },
    {  799,  853,  7 },
    // Refrain
    {  853,  899,  8 },
    {  899,  952,  9 },
    {  952, 1003, 10 },
    { 1003, 1055, 11 },
    { 1055, 1107, 12 },
    { 1107, 1158, 13 },
    { 1158, 1212, 14 },
    { 1212, 1264, 15 },
    // Chorus
    { 1264, 1314, 16 },
    { 1314, 1369, 17 },
    { 1369, 1420, 18 },
    { 1420, 1474, 19 },
    { 1474, 1524, 20 },
    { 1524, 1576, 21 },
    { 1576, 1630, 22 },
    { 1630, 1687, 23 },
    // Verse (repeat)
    { 1897, 1949,  0 },
    { 1949, 2002,  1 },
    { 2002, 2052,  2 },
    { 2052, 2106,  3 },
    { 2106, 2157,  4 },
    { 2157, 2209,  5 },
    { 2209, 2260,  6 },
    { 2260, 2313,  7 },
    // Refrain (repeat)
    { 2313, 2359,  8 },
    { 2359, 2412,  9 },
    { 2412, 2464, 10 },
    { 2464, 2516, 11 },
    { 2516, 2568, 12 },
    { 2568, 2621, 13 },
    { 2621, 2673, 14 },
    { 2673, 2724, 15 },
    // Outro
    { 2724, 2778, 24 },
    { 2778, 2830, 25 },
    { 2830, 2881, 26 },
    { 2881, 2933, 27 },
    { 2933, 2986, 28 },
    { 2986, 3037, 29 },
    { 3037, 3091, 30 },
    { 3091, 3150, 31 },
};
const uint8_t lyric_event_count = 48;


#endif /* LYRICS_H */