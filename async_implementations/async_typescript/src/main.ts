
async function add(a: number, b: number): Promise<number> {
    return new Promise((resolve) => {
        setTimeout(() => {
            resolve(a+b);
        }, 1000);
    })
}


async function main() {
    const result = (await add(4, 5)) + (await add(4, 5));

    console.log(result
        );
    return result;
}
    main()